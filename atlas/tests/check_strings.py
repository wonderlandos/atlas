#!/usr/bin/env python3
# Copyright 2026 The Atlas Authors
# GPL-3.0-or-later. Checks that every IDS_ATLAS_* message of chrome/app/atlas/*.grdp
# is in atlas/i18n/source.atlas.json and has a translation in each language of
# atlas/i18n/translations/ (same format as i18n/translations, applied by utils/i18n_apply.py).
import glob, json, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
LANGS = ['pt-BR', 'ja', 'fr', 'ru', 'it', 'de']
BANNED = re.compile(r'incognit|incógnit|utilisateur|usuário|usuario|benutzer|nutzer|пользовател|utente|ユーザー|навигация|navigation privée', re.I)
PH = re.compile(r'<ph name="([^"]+)"')


def grdp_messages():
    out = {}
    for f in sorted(glob.glob(os.path.join(ROOT, 'chrome/app/atlas/*.grdp'))):
        for m in re.finditer(r'<message name="(IDS_ATLAS_\w+)"(?:\s+\w+="[^"]*")*\s*>(.*?)</message>',
                             open(f, encoding='utf-8').read(), re.S):
            out[m.group(1)] = m.group(2).strip()
    return out


def main():
    errors = []
    grdp = grdp_messages()
    src = {e['name']: e for e in json.load(open(os.path.join(ROOT, 'atlas/i18n/source.atlas.json'), encoding='utf-8'))}
    for n, text in grdp.items():
        if n not in src or src[n]['message'] != text:
            errors.append(f'source.atlas.json out of date for {n}')
    for n in src:
        if n not in grdp:
            errors.append(f'source.atlas.json has stale {n}')
    for lang in LANGS:
        path = os.path.join(ROOT, 'atlas/i18n/translations', lang + '.json')
        if not os.path.exists(path):
            errors.append(f'{lang}: file missing')
            continue
        tr = {e['name']: e for e in json.load(open(path, encoding='utf-8'))}
        for n, text in grdp.items():
            e = tr.get(n)
            if not e or not e['message'].strip():
                errors.append(f'{lang}: no translation for {n}')
                continue
            if e['source'] != text:
                errors.append(f'{lang}: {n} source differs from the grdp')
            if sorted(PH.findall(e['message'])) != sorted(PH.findall(text)):
                errors.append(f'{lang}: {n} placeholders differ')
            if 'plural,' in text and 'plural,' not in e['message']:
                errors.append(f'{lang}: {n} lost the plural')
            if '&amp;' in text and '&amp;' not in e['message']:
                errors.append(f'{lang}: {n} lost the accelerator')
            if BANNED.search(e['message']):
                errors.append(f'{lang}: {n} uses a banned word')
        for n in tr:
            if n not in grdp:
                errors.append(f'{lang}: stale {n}')
    if errors:
        print('\n'.join(errors))
        return 1
    print(f'ok: {len(grdp)} messages x {len(LANGS)} languages')
    return 0


if __name__ == '__main__':
    sys.exit(main())
