"""Downloads Roland's SR-JV80 Waveform Listing and Patch Listing PDFs, as the owner explicitly allowed (including accepting the
download licence on the owner's behalf). Run from the repository root; the files stay local in artifact/references (git-ignored: Roland's licence forbids passing them on)."""
import re
import urllib.request
from pathlib import Path

OUT = Path(__file__).resolve().parents[1] / 'artifact' / 'references' / 'roland-sr-jv80'
OUT.mkdir(parents=True, exist_ok=True)

BASE = 'https://www.roland.com'


def get(u: str, binary: bool = False):
    data = urllib.request.urlopen(urllib.request.Request(u, headers={'User-Agent': 'Mozilla/5.0'}), timeout=60).read()
    return data if binary else data.decode('utf8', 'ignore')


log = []
for n in range(1, 20):
    listing = get(f'{BASE}/global/support/by_product/sr-jv80-{n:02d}/support_documents/')
    docs = re.findall(r'<a [^>]*href="([^"]*support_documents/[0-9a-f-]{36}/)"[^>]*>(.*?)</a>', listing, flags=re.S)
    seen = set()
    for href, title in docs:
        title = re.sub(r'<[^>]+>', '', title).strip()
        kind = 'wave' if 'aveform' in title else 'patch' if 'atch' in title else None
        if not kind or href in seen:
            continue
        seen.add(href)
        page = get(BASE + href).replace(chr(92) + '/', '/')
        m = re.search(r'https://static\.roland\.com/assets/media/pdf/[^&\'" ]*\.pdf', page)
        if not m:
            log.append(f'{n:02d} {kind}: no pdf link ({title})')
            continue
        pdf = get(m.group(0), True)
        name = f'sr-jv80-{n:02d}-{kind}-{len(seen)}.pdf'
        (OUT / name).write_bytes(pdf)
        log.append(f'{n:02d} {kind}: {m.group(0)} -> {name} ({len(pdf)} bytes)')
print('\n'.join(log))
