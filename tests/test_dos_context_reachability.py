import unittest

from tools.analyze_dos_context_reachability import (
    ContextReachabilityError,
    analyze_context_reachable,
    static_reference_candidates,
)


class DosContextReachabilityTests(unittest.TestCase):
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

    def test_state_and_depth_bounds_are_explicit(self):
        with self.assertRaises(ContextReachabilityError):
            analyze_context_reachable(b"\x90", state_limit=0)
        bounded = analyze_context_reachable(
            bytes.fromhex("e8fdff"), 0x100, call_depth_limit=1)
        self.assertEqual(bounded["first_blocked"]["reason"], "near-call-depth-limit")


if __name__ == "__main__":
    unittest.main()
