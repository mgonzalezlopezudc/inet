#!/usr/bin/env python3

"""Extract Appendix-3 Mean curves from a user-supplied 11-14/0571r12 XLSX.

The IEEE workbook and the emitted calibration data are intentionally not
distributed with INET. This converter uses only the Python standard library.
"""

import argparse
import re
import sys
import xml.etree.ElementTree as ET
import zipfile


MAIN_NS = "http://schemas.openxmlformats.org/spreadsheetml/2006/main"
REL_NS = "http://schemas.openxmlformats.org/officeDocument/2006/relationships"
NS = {"m": MAIN_NS, "r": REL_NS}


def column_number(reference):
    letters = re.match(r"[A-Z]+", reference).group(0)
    result = 0
    for letter in letters:
        result = result * 26 + ord(letter) - ord("A") + 1
    return result


def row_number(reference):
    return int(re.search(r"[0-9]+$", reference).group(0))


def read_workbook(path):
    archive = zipfile.ZipFile(path)
    shared_strings = []
    if "xl/sharedStrings.xml" in archive.namelist():
        root = ET.fromstring(archive.read("xl/sharedStrings.xml"))
        shared_strings = ["".join(node.itertext()) for node in root.findall("m:si", NS)]

    relationships = ET.fromstring(archive.read("xl/_rels/workbook.xml.rels"))
    relationship_targets = {node.attrib["Id"]: node.attrib["Target"] for node in relationships}
    workbook = ET.fromstring(archive.read("xl/workbook.xml"))
    sheets = {}
    for sheet in workbook.find("m:sheets", NS):
        relationship_id = sheet.attrib[f"{{{REL_NS}}}id"]
        target = relationship_targets[relationship_id].lstrip("/")
        if not target.startswith("xl/"):
            target = "xl/" + target
        root = ET.fromstring(archive.read(target))
        cells = {}
        for cell in root.findall(".//m:c", NS):
            value_node = cell.find("m:v", NS)
            if value_node is None:
                inline = cell.find("m:is", NS)
                value = "" if inline is None else "".join(inline.itertext())
            else:
                value = value_node.text
                if cell.attrib.get("t") == "s":
                    value = shared_strings[int(value)]
            cells[cell.attrib["r"]] = value
        sheets[sheet.attrib["name"]] = cells
    return sheets


def find_blocks(cells, expected_headers):
    row16 = {column_number(reference): value for reference, value in cells.items()
             if row_number(reference) == 16 and value}
    row18 = {column_number(reference): value for reference, value in cells.items()
             if row_number(reference) == 18 and value}
    blocks = []
    for column, header in sorted(row16.items()):
        if header not in expected_headers:
            continue
        mean_columns = [candidate for candidate, value in row18.items()
                        if value == "Mean" and column < candidate < column + 14]
        if len(mean_columns) != 1:
            raise ValueError(f"expected one Mean column for {header}, found {mean_columns}")
        blocks.append((header, column, mean_columns[0]))
    if {header for header, _, _ in blocks} != set(expected_headers):
        raise ValueError(f"missing headers: {set(expected_headers) - {item[0] for item in blocks}}")
    return blocks


def reference(column, row):
    letters = ""
    while column:
        column, remainder = divmod(column - 1, 26)
        letters = chr(ord("A") + remainder) + letters
    return f"{letters}{row}"


def curve_rows(cells, argument_column, value_column):
    rows = []
    for row in range(19, 10000):
        argument = cells.get(reference(argument_column, row))
        value = cells.get(reference(value_column, row))
        if argument is None:
            if rows:
                break
            continue
        if value is None:
            raise ValueError(f"incomplete curve row {row}")
        try:
            float(argument)
            float(value)
        except ValueError:
            if rows:
                break
            continue
        rows.append((argument, value))
    if len(rows) < 2:
        raise ValueError("curve contains fewer than two rows")
    return rows


def extract(input_path, output):
    sheets = read_workbook(input_path)
    required_sheets = {"RBIR-CM", "BCC,32byte", "BCC,1458byte", "LDPC,1458byte"}
    if not required_sheets.issubset(sheets):
        raise ValueError(f"workbook is missing sheets {required_sheets - set(sheets)}")

    output.write("# Appendix-3 Mean curves extracted from a user-supplied IEEE 802.11-14/0571r12 workbook.\n")
    modulation_names = {"BPSK": "BPSK", "QPSK": "QPSK", "16QAM": "QAM16",
                        "64QAM": "QAM64", "256QAM": "QAM256"}
    cells = sheets["RBIR-CM"]
    for header, argument_column, value_column in find_blocks(cells, modulation_names):
        for snr_db, mean_rbir in curve_rows(cells, argument_column, value_column):
            output.write(f"RBIR {modulation_names[header]} {snr_db} {mean_rbir}\n")

    for sheet_name, coding, packet_length in (
            ("BCC,32byte", "BCC", 32),
            ("BCC,1458byte", "BCC", 1458),
            ("LDPC,1458byte", "LDPC", 1458)):
        cells = sheets[sheet_name]
        headers = {f"MCS{mcs}" for mcs in range(10)}
        for header, argument_column, value_column in find_blocks(cells, headers):
            mcs = int(header[3:])
            for snr_db, mean_per in curve_rows(cells, argument_column, value_column):
                output.write(f"PER {coding} {packet_length} {mcs} {snr_db} {mean_per}\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("workbook", help="user-supplied embedded Appendix-3 .xlsx workbook")
    parser.add_argument("output", nargs="?", help="output calibration file (default: stdout)")
    arguments = parser.parse_args()
    try:
        if arguments.output:
            with open(arguments.output, "w", encoding="utf-8") as output:
                extract(arguments.workbook, output)
        else:
            extract(arguments.workbook, sys.stdout)
    except (OSError, KeyError, ValueError, zipfile.BadZipFile) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
