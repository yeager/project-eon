from tools.scan_deuteros_address_literals import find_address_literals


def test_finds_unaligned_absolute_long_and_24_bit_markers():
    data = bytes.fromhex("aa 00 04 07 7e bb 04 07 7e cc")
    hits = find_address_literals(data, 0x4077E)
    assert [(hit.encoding, hit.offsets) for hit in hits] == [
        ("absolute-long", (1,)),
        ("24-bit", (2, 6)),
    ]


def test_no_literal_match_is_empty():
    assert find_address_literals(bytes.fromhex("00 04 07 7f"), 0x4077E) == ()


def test_rejects_addresses_outside_68000_address_space():
    try:
        find_address_literals(b"", 0x1000000)
    except ValueError as error:
        assert "24 bits" in str(error)
    else:
        raise AssertionError("out-of-range address was accepted")
