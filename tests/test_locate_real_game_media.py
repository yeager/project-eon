from __future__ import annotations

import hashlib
import pathlib
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
from locate_real_game_media import archive_paths, inspect_archive  # noqa: E402


class RealGameMediaLocatorTests(unittest.TestCase):
    @staticmethod
    def _fat12_disk_with_exec(payload: bytes, entry_name: bytes = b"EXEC    TOS") -> bytes:
        sector_size = 512
        total_sectors = 720
        image = bytearray(total_sectors * sector_size)
        image[11:13] = sector_size.to_bytes(2, "little")
        image[13] = 2
        image[14:16] = (1).to_bytes(2, "little")
        image[16] = 2
        image[17:19] = (112).to_bytes(2, "little")
        image[19:21] = total_sectors.to_bytes(2, "little")
        image[21] = 0xF9
        image[22:24] = (3).to_bytes(2, "little")
        image[24:26] = (9).to_bytes(2, "little")
        image[26:28] = (2).to_bytes(2, "little")
        image[510:512] = b"\x55\xaa"
        root_lba = 1 + 2 * 3
        entry = bytearray(32)
        if len(entry_name) != 11:
            raise ValueError("FAT12 8.3 name must contain eleven bytes")
        entry[:11] = entry_name
        entry[11] = 0x20
        entry[26:28] = (2).to_bytes(2, "little")
        entry[28:32] = len(payload).to_bytes(4, "little")
        image[root_lba * sector_size : root_lba * sector_size + 32] = entry
        first_fat = sector_size
        second_fat = first_fat + 3 * sector_size
        for fat_start in (first_fat, second_fat):
            image[fat_start : fat_start + 5] = b"\xf9\xff\xff\xff\x0f"
        data_lba = 1 + 2 * 3 + 7
        image[data_lba * sector_size : data_lba * sector_size + len(payload)] = payload
        return bytes(image)

    @staticmethod
    def _stx_from_fat12_disk(image: bytes) -> bytes:
        sectors_per_track = int.from_bytes(image[24:26], "little")
        heads = int.from_bytes(image[26:28], "little")
        total_sectors = int.from_bytes(image[19:21], "little")
        tracks: list[bytes] = []
        for cylinder in range((total_sectors + sectors_per_track * heads - 1)
                              // (sectors_per_track * heads)):
            for side in range(heads):
                descriptors = bytearray()
                payload = bytearray()
                for sector_id in range(1, sectors_per_track + 1):
                    lba = (cylinder * heads + side) * sectors_per_track + sector_id - 1
                    if lba >= total_sectors:
                        break
                    relative = len(payload)
                    descriptors.extend(relative.to_bytes(4, "little"))
                    descriptors.extend(bytes(4))
                    descriptors.extend(bytes((cylinder, side, sector_id, 2)))
                    descriptors.extend(bytes(4))
                    start = lba * 512
                    payload.extend(image[start:start + 512])
                count = len(descriptors) // 16
                block_size = 16 + len(descriptors) + len(payload)
                header = bytearray(16)
                header[:4] = block_size.to_bytes(4, "little")
                header[8:10] = count.to_bytes(2, "little")
                header[14] = cylinder | (side << 7)
                tracks.append(bytes(header) + bytes(descriptors) + bytes(payload))
        header = bytearray(16)
        header[:4] = b"RSY\0"
        header[4:6] = (3).to_bytes(2, "little")
        header[10] = len(tracks)
        return bytes(header) + b"".join(tracks)

    def test_recursively_finds_archives_from_directory_arguments(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            nested = root / "Downloads" / "nested"
            nested.mkdir(parents=True)
            archive_path = nested / "candidate.zip"
            with zipfile.ZipFile(archive_path, "w") as archive:
                archive.writestr("disk.stx", b"synthetic locator fixture")
            (nested / "notes.txt").write_text("not an archive")

            self.assertEqual(list(archive_paths([root / "Downloads"])), [archive_path])

    def test_reports_hash_sector_occupancy_and_text_leads_without_extracting(self):
        payload = bytearray(1024)
        payload[16:28] = b"DEUTEROSDATA"
        payload[100:117] = b"Forschungsleiter."
        payload[512:520] = b"COLONY!!"
        with tempfile.TemporaryDirectory() as directory:
            archive_path = pathlib.Path(directory) / "candidate.zip"
            with zipfile.ZipFile(archive_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
                archive.writestr("candidate.adf", payload)

            result = inspect_archive(archive_path)

            member = result["members"][0]
            self.assertEqual(member["sha256"], hashlib.sha256(payload).hexdigest())
            self.assertEqual(member["nonempty_sector_count"], 2)
            self.assertEqual(member["deuterosdata_marker_offsets"], [16])
            self.assertEqual(
                member["german_briefing_marker_offsets"],
                {"Forschungsleiter.": [100]},
            )
            self.assertIn("COLONY", member["game_text_markers"])
            self.assertFalse((pathlib.Path(directory) / "candidate.adf").exists())

    def test_reports_atari_disk_member_identity_without_extracting_or_interpreting(self):
        payload = b"synthetic locator fixture; not game data"
        with tempfile.TemporaryDirectory() as directory:
            archive_path = pathlib.Path(directory) / "atari.zip"
            with zipfile.ZipFile(archive_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
                archive.writestr("Disk 1.stx", payload)

            result = inspect_archive(archive_path)

            self.assertEqual(len(result["members"]), 1)
            self.assertEqual(result["members"][0]["media_kind"], "atari_st_stx")
            self.assertEqual(result["members"][0]["size"], len(payload))
            self.assertEqual(result["members"][0]["sha256"], hashlib.sha256(payload).hexdigest())
            self.assertFalse(result["members"][0]["content_inspected"])
            self.assertFalse((pathlib.Path(directory) / "Disk 1.stx").exists())

    def test_hashes_rooted_prg_candidates_in_atari_st_images_without_writing_them(self):
        executable = b"real-data-search-marker"
        payload = self._fat12_disk_with_exec(executable)
        with tempfile.TemporaryDirectory() as directory:
            archive_path = pathlib.Path(directory) / "atari.zip"
            with zipfile.ZipFile(archive_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
                archive.writestr("Disk 1.st", payload)

            result = inspect_archive(archive_path)

            member = result["members"][0]
            self.assertTrue(member["content_inspected"])
            self.assertTrue(member["fat12_root_inspected"])
            self.assertEqual(member["root_files"], [{
                "name": "EXEC.TOS",
                "size": len(executable),
                "sha256": hashlib.sha256(executable).hexdigest(),
                "chain_consensus": True,
            }])
            self.assertFalse((pathlib.Path(directory) / "Disk 1.st").exists())

    def test_hashes_stx_root_files_in_memory_to_find_real_media_candidates(self):
        executable = b"synthetic search marker, not an executable"
        disk = self._fat12_disk_with_exec(executable, b"2200AD  PRG")
        stx = self._stx_from_fat12_disk(disk)
        with tempfile.TemporaryDirectory() as directory:
            archive_path = pathlib.Path(directory) / "Atari STX.zip"
            with zipfile.ZipFile(archive_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
                archive.writestr("Disk 1.stx", stx)

            result = inspect_archive(archive_path)

            member = result["members"][0]
            self.assertEqual(member["media_kind"], "atari_st_stx")
            self.assertTrue(member["content_inspected"])
            self.assertTrue(member["fat12_root_inspected"])
            self.assertTrue(member["fat_mirrors_identical"])
            self.assertEqual(member["root_files"], [{
                "name": "2200AD.PRG",
                "size": len(executable),
                "sha256": hashlib.sha256(executable).hexdigest(),
                "chain_consensus": True,
            }])
            self.assertFalse((pathlib.Path(directory) / "Disk 1.stx").exists())

    def test_rejects_oversized_member(self):
        with tempfile.TemporaryDirectory() as directory:
            archive_path = pathlib.Path(directory) / "oversized.zip"
            with zipfile.ZipFile(archive_path, "w") as archive:
                archive.writestr("large.adf", b"x" * (4 * 1024 * 1024 + 1))

            with self.assertRaisesRegex(ValueError, "outside locator bounds"):
                inspect_archive(archive_path)


if __name__ == "__main__":
    unittest.main()
