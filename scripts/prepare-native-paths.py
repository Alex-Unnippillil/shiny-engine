#!/usr/bin/env python3
"""Adapt only Windows UTF-8 file-path opening in pinned MIT OpenDLSS sources.

No graph, weights, arithmetic or kernels are changed. Refuse unrecognized input.
Generated files stay in the build directory; original sources remain untouched.
"""
import argparse
import hashlib
from pathlib import Path

PINS = {
    'nr_model.cpp': 'cce57c0b03f3b9a95b5fdbc3719d17a20355c664bb871cbaec917a3d0f84330c',
    'kernels.cpp': 'fec331a009e8a325c90fbed54f3d7e7dab66ba53bc1dec71c38d1271eed3cb89',
    'vk_context.cpp': '524064ee836ab8599a39d9686531e05889f044f7116cb0ce66cdb3ad2e424690',
}
REPLACEMENTS = {
    'nr_model.cpp': [('std::ifstream file(path,', 'std::ifstream file(std::filesystem::u8path(path),', 2)],
    'vk_context.cpp': [('std::ifstream file(spvPath,', 'std::ifstream file(std::filesystem::u8path(spvPath),', 1)],
    'kernels.cpp': [
        ('std::ifstream probe(ptxDirectory_ + "/" + entry + ".ptx");', 'std::ifstream probe(std::filesystem::u8path(ptxDirectory_ + "/" + entry + ".ptx"));', 2),
        ('std::ifstream in(ptxDirectory_ + "/" + file);', 'std::ifstream in(std::filesystem::u8path(ptxDirectory_ + "/" + file));', 1),
        ('std::ifstream(ptxDirectory_ + "/global_attention_stream_e4m3.ptx")', 'std::ifstream(std::filesystem::u8path(ptxDirectory_ + "/global_attention_stream_e4m3.ptx"))', 1),
        ('std::ifstream(ptxDirectory_ + "/global_normalize_e4m3.ptx")', 'std::ifstream(std::filesystem::u8path(ptxDirectory_ + "/global_normalize_e4m3.ptx"))', 1),
    ],
}


def adapt(source: Path, destination: Path) -> None:
    prepared = {}
    for name, expected in PINS.items():
        original = (source / 'src' / name).read_bytes().replace(b'\r\n', b'\n')
        if hashlib.sha256(original).hexdigest() != expected:
            raise ValueError(f'Pinned native source changed: {name}')
        text = original.decode('utf-8')
        for before, after, count in REPLACEMENTS[name]:
            if text.count(before) != count:
                raise ValueError(f'Unrecognized file-path reader in {name}')
            text = text.replace(before, after)
        prepared[name] = '// Shiny modification: UTF-8 filesystem paths only; upstream MIT notices apply.\n#include <filesystem>\n' + text
    destination.mkdir(parents=True, exist_ok=True)
    for name, text in prepared.items():
        (destination / name).write_text(text, encoding='utf-8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    adapt(args.source, args.destination)
