# Fixture source and license notices

`mqb-candidate-subset.dbc` is a small test fixture derived from signal
definitions in commaai/opendbc's `opendbc/dbc/vw_mqb.dbc`, pinned at commit
`c4465696dc2ef0d1340ae188810ac982586999a4`:

https://github.com/commaai/opendbc/blob/c4465696dc2ef0d1340ae188810ac982586999a4/opendbc/dbc/vw_mqb.dbc

The upstream DBC is distributed under the MIT License. The fixture contains
only a few message and signal definitions needed by offline tests; it contains
no vehicle capture. The full upstream license notice follows.

Copyright (c) 2020, Comma.ai, Inc.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## Typed MCD scalar fixture

`mcd-typed-scalars.json` is the preserved synthetic local-only fixture recovered
from the original checkout. Its illustrative 0x2201/0x2202/0x2203 values test
IEEE754 byte order and TEXTTABLE rejection; they are not vehicle DIDs or route
evidence. It contains no proprietary ODIS database. The importer still accepts
only byte-aligned single leaf values and retains candidate/provenance gates.
