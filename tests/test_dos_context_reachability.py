import unittest

from capstone import CS_ARCH_X86, CS_MODE_16, Cs

from tools.analyze_dos_context_reachability import (
    ContextReachabilityError,
    TITLES_SHA256,
    TITLES_SIZE,
    _memory_may_alias_return_word,
    analyze_context_reachable,
    indirect_control_flow_candidates,
    static_reference_candidates,
)


class DosContextReachabilityTests(unittest.TestCase):
    def test_dosboxx_com_stack_interval_proves_011e_disjoint(self):
        decoder = Cs(CS_ARCH_X86, CS_MODE_16)
        decoder.detail = True
        operand = next(decoder.disasm(bytes.fromhex("a31e01"), 0x100)).operands[0]
        self.assertFalse(_memory_may_alias_return_word(
            operand, ((0x1B46, -4),), ss_equals_cs=True,
            stack_pointer_base=0x1D6E, stack_pointer_base_max=0xFFFE,
            ds_equals_cs=True, si_values=None))

    def test_dosboxx_com_stack_interval_detects_possible_alias(self):
        decoder = Cs(CS_ARCH_X86, CS_MODE_16)
        decoder.detail = True
        operand = next(decoder.disasm(bytes.fromhex("a3701d"), 0x100)).operands[0]
        self.assertTrue(_memory_may_alias_return_word(
            operand, ((0x1B46, -4),), ss_equals_cs=True,
            stack_pointer_base=0x1D6E, stack_pointer_base_max=0xFFFE,
            ds_equals_cs=True, si_values=None))

    def test_bx_relative_cs_write_uses_known_immediate_value(self):
        decoder = Cs(CS_ARCH_X86, CS_MODE_16)
        decoder.detail = True
        operand = next(decoder.disasm(bytes.fromhex("2e894702"), 0x100)).operands[0]
        args = dict(ss_equals_cs=True, stack_pointer_base=0x1D6E,
                    stack_pointer_base_max=0xFFFE, ds_equals_cs=True,
                    si_values=None)
        self.assertFalse(_memory_may_alias_return_word(
            operand, ((0x1B46, -4),), bx_constant=0x010C, **args))
        self.assertTrue(_memory_may_alias_return_word(
            operand, ((0x1B46, -4),), bx_constant=0x1D68, **args))
        self.assertTrue(_memory_may_alias_return_word(
            operand, ((0x1B46, -4),), bx_constant=None, **args))
        default_segment = next(
            decoder.disasm(bytes.fromhex("894702"), 0x100)).operands[0]
        self.assertTrue(_memory_may_alias_return_word(
            default_segment, ((0x1B46, -4),), bx_constant=0x010C,
            ss_equals_cs=True, stack_pointer_base=0x1D6E,
            stack_pointer_base_max=0xFFFE, ds_equals_cs=None, si_values=None))

    def test_bx_immediate_is_tracked_through_near_call(self):
        # MOV BX,010c immediately precedes the callee's CS:[BX+2] write;
        # with SS=CS and SP=da00, that write is disjoint from the return word.
        code = bytes.fromhex("8cc88ed0b800da89c4bb0c01e8020090902e894702c3")
        result = analyze_context_reachable(code, 0x0100)
        self.assertIn(0x0111, result["instructions"])
        self.assertIn((0x0115, 0x010F, "matched-near-ret", 0), result["edges"])
        self.assertEqual(result["first_blocked"]["reason"],
                         "near-ret-without-call-frame")

    def test_dosboxx_com_stack_scenario_rejects_unverified_image(self):
        self.assertEqual(TITLES_SIZE, 7022)
        self.assertEqual(len(TITLES_SHA256), 64)
        with self.assertRaises(ContextReachabilityError):
            analyze_context_reachable(b"\x90" * TITLES_SIZE,
                                      dosboxx_com_loader_stack=True)

    def test_near_ret_matches_call_frame_and_returns_to_caller(self):
        # CALL 0105; INT 91; callee RET. The return is matched to 0103.
        result = analyze_context_reachable(bytes.fromhex("e80200cd91c3"), 0x100)
        self.assertIn(0x105, result["instructions"])
        self.assertIn(0x103, result["instructions"])
        self.assertIn((0x105, 0x103, "matched-near-ret", 0), result["edges"])
        self.assertEqual(result["first_blocked"]["ip"], 0x103)
        self.assertEqual(result["first_blocked"]["reason"], "software-interrupt-opaque")
        self.assertEqual(result["first_blocked"]["interrupt_vector"], 0x91)
        self.assertEqual(result["first_blocked"]["call_stack"], [])

    def test_unmatched_near_ret_stops_without_fallthrough(self):
        result = analyze_context_reachable(b"\xc3\x90", 0x100)
        self.assertNotIn(0x101, result["instructions"])
        self.assertEqual(result["first_blocked"]["reason"], "near-ret-without-call-frame")

    def test_near_ret_rejects_unbalanced_stack_depth(self):
        # CALL 0105; callee PUSH AX then RET would pop AX, not the return IP.
        result = analyze_context_reachable(bytes.fromhex("e80200909050c3"), 0x100)
        self.assertNotIn(0x103, result["instructions"])
        self.assertEqual(result["first_blocked"]["reason"],
                         "near-ret-stack-delta-mismatch")

    def test_memory_operand_stops_while_return_slot_aliasing_is_unmodeled(self):
        # CALL 0105; callee MOV [0103],AX could alias the return word. Even
        # though this absolute address is not the modeled SS:SP, aliasing is
        # unknown, so no RET is followed while a call frame is active.
        result = analyze_context_reachable(bytes.fromhex("e802009090a30301c3"), 0x100)
        self.assertNotIn(0x103, result["instructions"])
        self.assertEqual(result["first_blocked"]["ip"], 0x105)
        self.assertEqual(result["first_blocked"]["reason"],
                         "memory-operand-with-active-call-frame")

    def test_cs_absolute_memory_operand_follows_only_when_return_slot_is_disjoint(self):
        # Establish SS=CS and SP=$da00, CALL a body that writes CS:$0107,
        # then RET. The absolute byte is disjoint from the return word at
        # CS:$d9fe, so this memory access cannot corrupt the active frame.
        code = bytes.fromhex("8cc88ed0b800da89c4e8020090902ea20701c3")
        result = analyze_context_reachable(code, 0x0100)
        self.assertIn(0x0112, result["instructions"])
        self.assertEqual(result["first_blocked"]["reason"],
                         "near-ret-without-call-frame")

    def test_cs_absolute_memory_operand_stops_when_it_overlaps_return_slot(self):
        # CS:$d9fe is the caller return word after the exact SP setup and CALL.
        code = bytes.fromhex("8cc88ed0b800da89c4e8020090902ea2fed9c3")
        result = analyze_context_reachable(code, 0x0100)
        self.assertEqual(result["first_blocked"]["ip"], 0x010E)
        self.assertEqual(result["first_blocked"]["reason"],
                         "memory-operand-with-active-call-frame")

    def test_cs_absolute_memory_operand_stops_without_proven_ss_relation(self):
        # A known SP baseline is insufficient while SS:SP cannot be compared
        # with CS:absolute; the stack alias relation must also be proven.
        code = bytes.fromhex("b800da89c4e8020090902ea20701c3")
        result = analyze_context_reachable(code, 0x0100)
        self.assertEqual(result["first_blocked"]["ip"], 0x010A)
        self.assertEqual(result["first_blocked"]["reason"],
                         "memory-operand-with-active-call-frame")

    def test_ds_absolute_memory_operand_follows_only_when_ds_and_ss_are_cs(self):
        code = bytes.fromhex("0e1f8cc88ed0b800da89c4e802009090a20701c3")
        result = analyze_context_reachable(code, 0x0100)
        self.assertIn(0x0110, result["instructions"])
        self.assertEqual(result["first_blocked"]["reason"],
                         "near-ret-without-call-frame")

        overlapping = bytes.fromhex("0e1f8cc88ed0b800da89c4e802009090a2fed9c3")
        rejected = analyze_context_reachable(overlapping, 0x0100)
        self.assertEqual(rejected["first_blocked"]["ip"], 0x0110)
        self.assertEqual(rejected["first_blocked"]["reason"],
                         "memory-operand-with-active-call-frame")

        unknown_ds = bytes.fromhex("8cc88ed0b800da89c4e802009090a20701c3")
        unknown = analyze_context_reachable(unknown_ds, 0x0100)
        self.assertEqual(unknown["first_blocked"]["ip"], 0x010E)
        self.assertEqual(unknown["first_blocked"]["reason"],
                         "memory-operand-with-active-call-frame")

    def test_ds_si_memory_read_follows_when_segment_and_range_are_known_disjoint(self):
        # LODSB reads CS:$0107 only after DS=CS, SI=$0107, SS=CS, and SP=$da00
        # are established; that byte cannot alias the active return word.
        code = bytes.fromhex("8cc88ed00e1fb800da89c4e80200f49090be0701acc3")
        result = analyze_context_reachable(code, 0x0100)
        self.assertIn(0x0114, result["instructions"])
        self.assertEqual(result["first_blocked"]["reason"], "terminal-instruction")

    def test_ds_si_memory_read_stops_when_si_may_alias_return_word(self):
        code = bytes.fromhex("8cc88ed00e1fb800da89c4e80200f49090befed9acc3")
        result = analyze_context_reachable(code, 0x0100)
        self.assertEqual(result["first_blocked"]["ip"], 0x0114)
        self.assertEqual(result["first_blocked"]["reason"],
                         "memory-operand-with-active-call-frame")

    def test_push_cs_symbol_does_not_survive_an_intervening_call(self):
        code = bytes.fromhex("0ee800001fc3")
        result = analyze_context_reachable(code, 0x0100)
        self.assertEqual(result["instructions"][0x0105]["mnemonic"], "ret")
        self.assertIsNone(result["first_blocked"]["ds_equals_cs"])

    def test_int_91_does_not_assume_a_return(self):
        result = analyze_context_reachable(bytes.fromhex("cd91c3"), 0x100)
        self.assertNotIn(0x102, result["instructions"])
        self.assertEqual(result["first_blocked"]["reason"], "software-interrupt-opaque")
        self.assertEqual(result["first_blocked"]["interrupt_vector"], 0x91)

    def test_bios_int10_stays_opaque_without_exact_site_scenario(self):
        data = bytearray(b"\x90" * (0x46D - 0x100))
        data.extend(bytes.fromhex("cd1090cd91"))
        result = analyze_context_reachable(bytes(data), 0x46D)
        self.assertNotIn(0x46F, result["instructions"])
        self.assertEqual(result["first_blocked"]["ip"], 0x46D)
        self.assertEqual(result["first_blocked"]["interrupt_vector"], 0x10)

    def test_bios_int10_scenario_is_site_limited_and_invalidates_tracked_state(self):
        data = bytearray(b"\x90" * (0x465 - 0x100))
        data.extend(bytes.fromhex("b405b007e80100c3cd10c3"))
        result = analyze_context_reachable(
            bytes(data), 0x465, return_from_bios_int10_at_046d=True)
        self.assertIn(0x46F, result["instructions"])
        self.assertIn((0x46F, 0x46C, "matched-near-ret", 0), result["edges"])
        self.assertEqual(result["first_blocked"]["ip"], 0x46C)
        self.assertEqual(result["first_blocked"]["reason"],
                         "near-ret-without-call-frame")
        self.assertIsNone(result["first_blocked"]["al_constant"])
        self.assertIsNone(result["first_blocked"]["ah_constant"])
        opaque = analyze_context_reachable(bytes(data), 0x465)
        self.assertEqual(opaque["first_blocked"]["al_constant"], 0x07)
        self.assertEqual(opaque["first_blocked"]["ah_constant"], 0x05)
        other_site = analyze_context_reachable(
            bytes.fromhex("cd10c3"), 0x100, return_from_bios_int10_at_046d=True)
        self.assertNotIn(0x102, other_site["instructions"])

    def test_bios_ds_is_preserved_only_in_its_exact_scenario(self):
        data = bytearray(b"\x90" * (0x46A - 0x100))
        data.extend(bytes.fromhex("0e1f90cd10cd91"))
        opaque = analyze_context_reachable(bytes(data), 0x46A)
        self.assertEqual(opaque["first_blocked"]["ip"], 0x46D)
        self.assertIs(opaque["first_blocked"]["ds_equals_cs"], True)

        cleared = analyze_context_reachable(
            bytes(data), 0x46A, return_from_bios_int10_at_046d=True)
        self.assertEqual(cleared["first_blocked"]["ip"], 0x46F)
        self.assertIsNone(cleared["first_blocked"]["ds_equals_cs"])

        preserved = analyze_context_reachable(
            bytes(data), 0x46A, return_from_bios_int10_at_046d=True,
            preserve_bios_ds_at_046d=True)
        self.assertTrue(preserved["bios_ds_preserved_at_046d"])
        self.assertIs(preserved["first_blocked"]["ds_equals_cs"], True)
        with self.assertRaises(ContextReachabilityError):
            analyze_context_reachable(bytes(data), 0x46A,
                                      preserve_bios_ds_at_046d=True)

    def test_dos_int21_return_scenario_is_exact_and_clears_register_facts(self):
        data = bytearray(b"\x90" * (0x1B22 - 0x100))
        data.extend(bytes.fromhex("b8014a90cd217502cd91cd93"))
        opaque = analyze_context_reachable(bytes(data), 0x1B22)
        self.assertEqual(opaque["first_blocked"]["ip"], 0x1B26)
        self.assertEqual(opaque["first_blocked"]["interrupt_vector"], 0x21)
        self.assertEqual(opaque["first_blocked"]["ax_constant"], 0x4A01)

        scenario = analyze_context_reachable(
            bytes(data), 0x1B22, return_from_dos_int21_at_1b26=True)
        stops = {(stop["ip"], stop.get("interrupt_vector"))
                 for stop in scenario["stops"]}
        self.assertEqual(stops, {(0x1B2A, 0x91), (0x1B2C, 0x93)})
        self.assertTrue(scenario["dos_int21_return_at_1b26"])
        self.assertTrue(all(stop["al_constant"] is None
                            and stop["ah_constant"] is None
                            for stop in scenario["stops"]))

        other_site = analyze_context_reachable(
            bytes.fromhex("cd21c3"), 0x100,
            return_from_dos_int21_at_1b26=True)
        self.assertNotIn(0x102, other_site["instructions"])

    def test_dos_ah48_return_scenario_is_exact_and_keeps_results_opaque(self):
        # DOS AH=48 at the reviewed allocator site may be modeled only as a
        # control-flow return; AX, flags, allocation and memory stay unknown.
        data = bytearray(b"\x90" * (0x1B2D - 0x100))
        data.extend(bytes.fromhex("cd2190cd91"))
        opaque = analyze_context_reachable(bytes(data), 0x1B2D)
        self.assertEqual(opaque["first_blocked"]["ip"], 0x1B2D)
        self.assertEqual(opaque["first_blocked"]["interrupt_vector"], 0x21)
        self.assertIsNone(opaque["first_blocked"]["ax_constant"])

        scenario = analyze_context_reachable(
            bytes(data), 0x1B2D, return_from_dos_int21_at_1b2d=True)
        self.assertTrue(scenario["dos_int21_return_at_1b2d"])
        self.assertEqual(scenario["first_blocked"]["ip"], 0x1B30)
        self.assertEqual(scenario["first_blocked"]["interrupt_vector"], 0x91)
        self.assertIsNone(scenario["first_blocked"]["ax_constant"])

        other_site = analyze_context_reachable(
            bytes.fromhex("cd2190"), 0x0100,
            return_from_dos_int21_at_1b2d=True)
        self.assertNotIn(0x0102, other_site["instructions"])

    def test_dos_free_return_scenario_is_site_specific_and_clears_flags(self):
        data = bytearray(b"\x90" * (0x1B38 - 0x100))
        data.extend(bytes.fromhex("cd21750190cd91"))
        opaque = analyze_context_reachable(bytes(data), 0x1B38)
        self.assertEqual(opaque["first_blocked"]["interrupt_vector"], 0x21)

        scenario = analyze_context_reachable(
            bytes(data), 0x1B38, return_from_dos_int21_at_1b38=True)
        self.assertTrue(scenario["dos_int21_return_at_1b38"])
        self.assertEqual(scenario["first_blocked"]["interrupt_vector"], 0x91)
        self.assertIsNone(scenario["first_blocked"]["zf_constant"])
        edge_kinds = {edge[2] for edge in scenario["edges"]}
        self.assertIn("conditional-taken-flags-unknown", edge_kinds)
        self.assertIn("conditional-fallthrough-flags-unknown", edge_kinds)

        wrong_site = analyze_context_reachable(
            bytes.fromhex("cd21"), 0x0100,
            return_from_dos_int21_at_1b38=True)
        self.assertNotIn(0x0102, wrong_site["instructions"])

    def test_dos_second_ah48_return_scenario_is_site_specific(self):
        data = bytearray(b"\x90" * (0x1B3F - 0x100))
        data.extend(bytes.fromhex("cd2190cd91"))
        scenario = analyze_context_reachable(
            bytes(data), 0x1B3F, return_from_dos_int21_at_1b3f=True)
        self.assertTrue(scenario["dos_int21_return_at_1b3f"])
        self.assertEqual(scenario["first_blocked"]["ip"], 0x1B42)
        self.assertEqual(scenario["first_blocked"]["interrupt_vector"], 0x91)

        wrong_site = analyze_context_reachable(
            bytes.fromhex("cd21"), 0x0100,
            return_from_dos_int21_at_1b3f=True)
        self.assertNotIn(0x0102, wrong_site["instructions"])

    def test_additional_dos_return_sites_clear_registers_and_are_allowlisted(self):
        data = bytearray(b"\x90" * (0x1B4F - 0x100))
        data.extend(bytes.fromhex("cd21750190cd91"))
        opaque = analyze_context_reachable(bytes(data), 0x1B4F)
        self.assertEqual(opaque["first_blocked"]["ip"], 0x1B4F)

        scenario = analyze_context_reachable(
            bytes(data), 0x1B4F, dos_int21_return_sites=(0x1B4F,))
        self.assertEqual(scenario["dos_int21_return_sites"], [0x1B4F])
        self.assertEqual(scenario["first_blocked"]["ip"], 0x1B54)
        self.assertEqual(scenario["first_blocked"]["interrupt_vector"], 0x91)
        self.assertIsNone(scenario["first_blocked"]["ax_constant"])

        with self.assertRaises(ContextReachabilityError):
            analyze_context_reachable(bytes(data), dos_int21_return_sites=(0x1B26,))
        with self.assertRaises(ContextReachabilityError):
            analyze_context_reachable(bytes(data), dos_int21_return_sites=(0x1B4F, 0x1B4F))

    def test_dos_ah48_return_at_1bca_preserves_only_the_caller_stack(self):
        code = bytearray(b"\x90" * (0x1BCA - 0x100))
        code.extend(bytes.fromhex("cd21750190cd21"))
        opaque = analyze_context_reachable(bytes(code), 0x1BCA)
        self.assertEqual(opaque["first_blocked"]["ip"], 0x1BCA)

        scenario = analyze_context_reachable(
            bytes(code), 0x1BCA, dos_int21_return_sites=(0x1BCA,))
        self.assertEqual(scenario["dos_int21_return_sites"], [0x1BCA])
        self.assertEqual(scenario["first_blocked"]["ip"], 0x1BCF)
        self.assertEqual(scenario["first_blocked"]["interrupt_vector"], 0x21)
        self.assertIsNone(scenario["first_blocked"]["ax_constant"])
        self.assertIsNone(scenario["first_blocked"]["bx_constant"])

        with self.assertRaises(ContextReachabilityError):
            analyze_context_reachable(bytes(code), 0x1BCA,
                                      dos_int21_return_sites=(0x1BCB,))

    def test_dos_ah49_return_at_1bd5_checks_exact_preimage_and_clears_results(self):
        code = bytearray(b"\x90" * (0x1BD5 - 0x100 + 6))
        start = 0x1BD3 - 0x100
        code[start:start + 6] = bytes.fromhex("b449cd21cd91")
        opaque = analyze_context_reachable(bytes(code), 0x1BD5)
        self.assertEqual(opaque["first_blocked"]["ip"], 0x1BD5)

        scenario = analyze_context_reachable(
            bytes(code), 0x1BD5, dos_int21_return_sites=(0x1BD5,))
        self.assertEqual(scenario["dos_int21_return_sites"], [0x1BD5])
        self.assertEqual(scenario["first_blocked"]["ip"], 0x1BD7)
        self.assertEqual(scenario["first_blocked"]["interrupt_vector"], 0x91)
        self.assertIsNone(scenario["first_blocked"].get("ax_constant"))

        invalid_preimage = bytearray(code)
        invalid_preimage[start + 1] = 0x48
        with self.assertRaisesRegex(ContextReachabilityError, "exact 0x1bd3 preimage"):
            analyze_context_reachable(
                bytes(invalid_preimage), 0x1BD5, dos_int21_return_sites=(0x1BD5,))

    def test_dos_ah3d_return_at_0549_checks_exact_preimage_and_clears_results(self):
        code = bytearray(b"\x90" * (0x0545 - 0x100 + 8))
        start = 0x0545 - 0x100
        code[start:start + 8] = bytes.fromhex("b002b43dcd21cd91")
        opaque = analyze_context_reachable(bytes(code), 0x0549)
        self.assertEqual(opaque["first_blocked"]["ip"], 0x0549)

        scenario = analyze_context_reachable(
            bytes(code), 0x0549, dos_int21_return_sites=(0x0549,))
        self.assertEqual(scenario["dos_int21_return_sites"], [0x0549])
        self.assertEqual(scenario["first_blocked"]["ip"], 0x054b)
        self.assertEqual(scenario["first_blocked"]["interrupt_vector"], 0x91)
        self.assertIsNone(scenario["first_blocked"].get("ax_constant"))

        invalid_preimage = bytearray(code)
        invalid_preimage[start + 3] = 0x48
        with self.assertRaisesRegex(ContextReachabilityError, "exact 0x0545 preimage"):
            analyze_context_reachable(
                bytes(invalid_preimage), 0x0549, dos_int21_return_sites=(0x0549,))

    def test_observed_int91_scenario_tracks_al_and_known_jne_without_runtime_claim(self):
        # The only injected scenario boundary is the observed TITLES.EXE
        # wrapper at $0127. The values select distinct locally decoded paths;
        # this does not claim either path was followed by the runtime.
        code = b"\x90" * 0x27 + bytes.fromhex("cd913c017502cd93cd92")
        first = analyze_context_reachable(
            code, 0x0127, int91_return_ax_sequence=(0x0101, 0x0000))
        second = analyze_context_reachable(
            code, 0x0127, int91_return_ax_sequence=(0x0000,))
        first_stops = {(stop["ip"], stop.get("interrupt_vector")) for stop in first["stops"]}
        second_stops = {(stop["ip"], stop.get("interrupt_vector")) for stop in second["stops"]}
        self.assertIn((0x012D, 0x93), first_stops)
        self.assertNotIn((0x012F, 0x92), first_stops)
        self.assertIn((0x012F, 0x92), second_stops)
        self.assertNotIn((0x012D, 0x93), second_stops)
        self.assertEqual(first["int91_return_count_consumed"], 1)
        self.assertIn("scenario-int91-return-ax-0101",
                      {edge[2] for edge in first["edges"]})

    def test_int91_wrapper_stack_scenario_restores_only_saved_ds_at_exact_pop(self):
        data = bytearray(b"\x90" * 0x40)
        data[0x10:0x15] = bytes.fromhex("0e1f") + bytes.fromhex("e80d00")
        data[0x15] = 0xC3
        data[0x22:0x22 + 13] = bytes.fromhex("1e56575506cd91075d5f5e1fc3")
        preserved = analyze_context_reachable(
            bytes(data), 0x0110, int91_return_ax_sequence=(0x0101,),
            preserve_int91_wrapper_stack=True)
        self.assertTrue(preserved["int91_wrapper_stack_preserved"])
        self.assertEqual(preserved["first_blocked"]["ip"], 0x0115)
        self.assertIs(preserved["first_blocked"]["ds_equals_cs"], True)

        opaque = analyze_context_reachable(
            bytes(data), 0x0110, int91_return_ax_sequence=(0x0101,))
        self.assertIsNone(opaque["first_blocked"]["ds_equals_cs"])

    def test_int91_wrapper_stack_scenario_requires_exact_wrapper_and_return_values(self):
        with self.assertRaises(ContextReachabilityError):
            analyze_context_reachable(b"\xcd\x91", 0x0100,
                                      preserve_int91_wrapper_stack=True)
        data = bytearray(b"\x90" * 0x40)
        with self.assertRaises(ContextReachabilityError):
            analyze_context_reachable(bytes(data), 0x0100,
                                      int91_return_ax_sequence=(1,),
                                      preserve_int91_wrapper_stack=True)

    def test_int91_scenario_is_bounded_to_two_16_bit_returns(self):
        with self.assertRaises(ContextReachabilityError):
            analyze_context_reachable(b"\xcd\x91", 0x0100,
                                      int91_return_ax_sequence=(1, 2, 3))
        with self.assertRaises(ContextReachabilityError):
            analyze_context_reachable(b"\xcd\x91", 0x0100,
                                      int91_return_ax_sequence=(0x10000,))

    def test_unknown_conditional_flags_fork_both_paths(self):
        # JZ can reach either INT 91 or INT 93; neither is assumed to return.
        result = analyze_context_reachable(bytes.fromhex("7402cd91cd93"), 0x100)
        self.assertIn(0x102, result["instructions"])
        self.assertIn(0x104, result["instructions"])
        kinds = {edge[2] for edge in result["edges"]}
        self.assertIn("conditional-taken-flags-unknown", kinds)
        self.assertIn("conditional-fallthrough-flags-unknown", kinds)
        self.assertEqual({stop["interrupt_vector"] for stop in result["stops"]}, {0x91, 0x93})

    def test_nested_near_calls_pop_in_lifo_order(self):
        # 0100 CALL 0106; 0106 CALL 0109; the RET at 0109 returns first to
        # itself (the inner call's return IP), then to 0103 (the outer one).
        data = bytes.fromhex("e80300c39090e80000c3")
        result = analyze_context_reachable(data, 0x100)
        self.assertIn((0x109, 0x109, "matched-near-ret", 1), result["edges"])
        self.assertIn((0x109, 0x103, "matched-near-ret", 0), result["edges"])

    def test_static_references_are_labelled_as_candidates(self):
        # MOV AX,125c and an unaligned little-endian occurrence.
        refs = static_reference_candidates(bytes.fromhex("b85c12905c12"), 0x125C)
        self.assertTrue(any(row["kind"] == "byte-start-immediate-candidate"
                            and row["offset"] == 0 for row in refs))
        self.assertTrue(any(row["kind"] == "literal-little-endian-word"
                            and row["offset"] == 4 for row in refs))

    def test_loader_candidates_are_reported_as_reachability_targets(self):
        for target in (0x104A, 0x1247):
            with self.subTest(target=target):
                code = bytearray(target - 0x100 + 1)
                relative = target - (0x100 + 3)
                code[:3] = b"\xe9" + relative.to_bytes(2, "little")
                code[target - 0x100] = 0xF4
                result = analyze_context_reachable(bytes(code), 0x100)
                self.assertTrue(result["targets"][target]["reachable"])

    def test_absolute_pointer_candidates_report_read_and_write_direction(self):
        # The loader candidate reads CS:$011c with LDS. An alternate byte
        # stream can encode a write there; both remain candidates, not proof
        # that either byte offset is an executed instruction.
        read = static_reference_candidates(bytes.fromhex("2ec5161c01"), 0x011C)
        read_operands = [row for row in read
                         if row["kind"] == "byte-start-absolute-memory-candidate"]
        self.assertEqual(len(read_operands), 2)
        self.assertTrue(all(row["access"] == "read" for row in read_operands))
        self.assertTrue(any(row["segment"] == "cs" for row in read_operands))

        write = static_reference_candidates(bytes.fromhex("2ea31c01"), 0x011C)
        write_operands = [row for row in write
                          if row["kind"] == "byte-start-absolute-memory-candidate"]
        self.assertEqual(len(write_operands), 2)
        self.assertEqual(write_operands[0]["access"], "write")

    def test_indirect_and_far_transfer_candidates_remain_unresolved(self):
        # JMP [BX+SI] and far CALL [BX+SI] are retained as unresolved
        # candidates, while a near direct JMP is handled by the edge scanner.
        candidates = indirect_control_flow_candidates(
            bytes.fromhex("ff20 ff18 e90000"))
        self.assertEqual([(row["mnemonic"], row["operand_kind"])
                          for row in candidates],
                         [("jmp", "memory"), ("lcall", "memory")])

    def test_state_and_depth_bounds_are_explicit(self):
        with self.assertRaises(ContextReachabilityError):
            analyze_context_reachable(b"\x90", state_limit=0)
        bounded = analyze_context_reachable(
            bytes.fromhex("e8fdff"), 0x100, call_depth_limit=1)
        self.assertEqual(bounded["first_blocked"]["reason"], "near-call-depth-limit")


if __name__ == "__main__":
    unittest.main()
