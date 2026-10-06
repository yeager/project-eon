import unittest

from tools.analyze_dos_reachability import ReachabilityError, analyze_reachable


class DosReachabilityTests(unittest.TestCase):
    def test_conditional_branch_explores_both_successors(self):
        # JZ reaches IP 0104; fallthrough reaches the two NOPs first.
        result = analyze_reachable(b"\x74\x02\x90\x90\x90\xc3", 0x100)
        instructions = result["instructions"]
        self.assertIn(0x102, instructions)
        self.assertIn(0x104, instructions)
        self.assertEqual(result["route_to"](0x104)[0]["kind"], "conditional-taken")

    def test_direct_call_target_and_return_assumption_are_labeled(self):
        # CALL 0107; caller continues at 0103; callee returns at 0108.
        result = analyze_reachable(b"\xe8\x04\x00\x90\xc3\x90\x90\xc3", 0x100)
        kinds = {(edge["source_ip"], edge["target_ip"], edge["kind"])
                 for edge in result["edges"]}
        self.assertIn((0x100, 0x107, "direct-call-entry"), kinds)
        self.assertIn((0x100, 0x103, "direct-call-return-assumption"), kinds)
        self.assertIn(0x103, result["instructions"])
        self.assertIn(0x107, result["instructions"])
        self.assertIsNotNone(result["route_to"](0x107, False))
        self.assertIsNone(result["route_to"](0x103, False))

    def test_interrupt_return_is_explicitly_assumed(self):
        result = analyze_reachable(b"\xcd\x91\xc3", 0x100)
        self.assertIn((0x100, 0x102, "interrupt-return-assumption"),
                      {(edge["source_ip"], edge["target_ip"], edge["kind"])
                       for edge in result["edges"]})

    def test_indirect_jump_stops_without_linear_fallthrough(self):
        result = analyze_reachable(b"\xff\xe0\x90\xc3", 0x100)
        self.assertNotIn(0x102, result["instructions"])
        self.assertIn({"ip": 0x100, "reason": "opaque-indirect-jump"}, result["stops"])

    def test_far_jump_stops_instead_of_taking_near_or_fallthrough_edge(self):
        result = analyze_reachable(bytes.fromhex("ea0501341290c3"), 0x100)
        self.assertNotIn(0x105, result["instructions"])
        self.assertEqual(result["stops"], [{"ip": 0x100, "reason": "opaque-far-jump"}])

    def test_far_call_target_is_not_mistaken_for_near_in_image_call(self):
        # The segment word deliberately looks like an in-image IP.
        result = analyze_reachable(bytes.fromhex("9a0500010190c3"), 0x100)
        self.assertNotIn(0x101, result["instructions"])
        self.assertIn({"ip": 0x100, "reason": "opaque-call-target"}, result["stops"])
        self.assertIn((0x100, 0x105, "opaque-call-return-assumption"),
                      {(edge["source_ip"], edge["target_ip"], edge["kind"])
                       for edge in result["edges"]})

    def test_rejects_invalid_entry_and_limit(self):
        with self.assertRaises(ReachabilityError):
            analyze_reachable(b"\x90", 0x10000)
        with self.assertRaises(ReachabilityError):
            analyze_reachable(b"\x90", 0x100, 0)


if __name__ == "__main__":
    unittest.main()
