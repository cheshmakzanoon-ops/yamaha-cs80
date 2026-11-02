#!/usr/bin/env python3
"""Render the six-page completion handover from explicitly supplied evidence.

Requires ReportLab (python3 -m pip install reportlab). No network requests are
made. Paths and URLs in the input remain evidence references, not fetched data.

Usage:
  python3 scripts/build-completion-handover.py --data /path/to/final-data.json
  python3 scripts/build-completion-handover.py --data /path/to/draft.json \
      --out /tmp/handover-preview.pdf --draft

The JSON object supplies: date, repository, subtitle, assessment (paragraphs),
provenance (source_sha, v2_pin, merge_sha, pr_url, release_url,
release_source_sha, release_status, ci [{label, status, sha, url}]),
scorecard [{metric, before, after, target, state}], measurement_notes,
workstreams [{title, status, text, commits}], compatibility
[{capture, baseline, result}], audio_sequence [{change, audio, pin, evidence}],
evidence_notes, remaining [{title, text}], owner_only (strings), commands
(shell strings), and references [{label, url}]. All fields are plain text;
only the explicit URL fields become clickable links. No metric or success
claim is inferred by this renderer. The scorecard and all status words come
from the data file. Pending placeholders require --draft. The output replaces
the requested file only after a complete, exactly six-page render succeeds.
"""

from __future__ import annotations

import argparse
from html import escape
import json
from pathlib import Path
import re
import tempfile
import textwrap

import reportlab
from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen import canvas
from reportlab.platypus import (
    Flowable, KeepTogether, PageBreak, Paragraph, Preformatted,
    SimpleDocTemplate, Spacer, Table, TableStyle,
)


INK = colors.HexColor('#172A3A')
MUTED = colors.HexColor('#4A5C68')
BLUE = colors.HexColor('#21546C')
PALE = colors.HexColor('#EDF3F5')
LINE = colors.HexColor('#CFDBDF')
AMBER = colors.HexColor('#80521B')
WHITE = colors.white
PAGE_W, PAGE_H = A4
MARGIN = 47
WIDTH = PAGE_W - 2 * MARGIN
PAGE_COUNT = 6

# ReportLab ships these fonts, so the PDF embeds stable glyphs without relying
# on platform font substitution or an extra font installation.
FONT_DIR = Path(reportlab.__file__).resolve().parent / 'fonts'
pdfmetrics.registerFont(TTFont('CS80Body', str(FONT_DIR / 'Vera.ttf')))
pdfmetrics.registerFont(TTFont('CS80Bold', str(FONT_DIR / 'VeraBd.ttf')))
pdfmetrics.registerFontFamily('CS80Body', normal='CS80Body', bold='CS80Bold')


def clean(value: object) -> str:
    """Keep punctuation portable in the standard PDF fonts."""
    text = str(value)
    for old, new in {
        '\u2010': '-', '\u2011': '-', '\u2012': '-', '\u2013': '-',
        '\u2014': '-', '\u2212': '-', '\u00a0': ' ', '\u202f': ' ',
        '\u2018': "'", '\u2019': "'", '\u201c': '"', '\u201d': '"',
        '\u2192': ' -> ', '\u2264': '<=', '\u2265': '>=', '\u00d7': 'x',
        '\u03a9': 'ohm', '\u03bc': 'u', '\u00b5': 'u',
    }.items():
        text = text.replace(old, new)
    # Fail rather than silently render a missing-glyph box.
    try:
        text.encode('cp1252')
    except UnicodeEncodeError as exc:
        raise ValueError(f'Unsupported PDF-font character in {text!r}') from exc
    return text


def markup(value: object) -> str:
    return escape(clean(value)).replace('\n', '<br/>')


def linked(label: str, url: str) -> str:
    if not url.startswith('https://') or any(c in url for c in '\r\n'):
        raise ValueError(f'Evidence link must be an HTTPS URL: {url!r}')
    return f'<link href="{escape(url, quote=True)}" color="#21546C">{markup(label)}</link>'


STYLES = getSampleStyleSheet()
STYLES.add(ParagraphStyle('BodyCS', fontName='CS80Body', fontSize=9.5,
    leading=13.2, textColor=INK, spaceAfter=7, allowWidows=0, allowOrphans=0))
STYLES.add(ParagraphStyle('SmallCS', parent=STYLES['BodyCS'], fontSize=8.3,
    leading=11.1, spaceAfter=5))
STYLES.add(ParagraphStyle('TableCS', parent=STYLES['BodyCS'], fontSize=8.05,
    leading=10.5, spaceAfter=0))
STYLES.add(ParagraphStyle('TableHeadCS', parent=STYLES['TableCS'],
    fontName='CS80Bold', textColor=WHITE))
STYLES.add(ParagraphStyle('H1CS', fontName='CS80Bold', fontSize=21,
    leading=25, textColor=INK, spaceAfter=10, keepWithNext=True))
STYLES.add(ParagraphStyle('H2CS', fontName='CS80Bold', fontSize=11.8,
    leading=15, textColor=BLUE, spaceBefore=9, spaceAfter=5, keepWithNext=True))
STYLES.add(ParagraphStyle('EyebrowCS', fontName='CS80Bold', fontSize=8.6,
    leading=11, textColor=BLUE, spaceAfter=8, tracking=1.1))
STYLES.add(ParagraphStyle('CodeCS', fontName='CS80Body', fontSize=7.45,
    leading=10.3, textColor=INK, leftIndent=0, spaceAfter=0))


def para(text: object, style: str = 'BodyCS', *, raw: bool = False) -> Paragraph:
    return Paragraph(str(text) if raw else markup(text), STYLES[style])


def grid(headers: list[str], rows: list[list[object]], fractions: list[float],
         *, font_style: str = 'TableCS') -> Table:
    cells = [[para(h, 'TableHeadCS') for h in headers]]
    cells.extend([[x if isinstance(x, Flowable) else para(x, font_style)
                   for x in row] for row in rows])
    table = Table(cells, colWidths=[WIDTH * f for f in fractions],
                  repeatRows=1, hAlign='LEFT')
    table.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), BLUE),
        ('ROWBACKGROUNDS', (0, 1), (-1, -1), [WHITE, PALE]),
        ('VALIGN', (0, 0), (-1, -1), 'TOP'),
        ('LEFTPADDING', (0, 0), (-1, -1), 7),
        ('RIGHTPADDING', (0, 0), (-1, -1), 7),
        ('TOPPADDING', (0, 0), (-1, -1), 7),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 7),
        ('LINEBELOW', (0, 0), (-1, 0), 0.6, BLUE),
        ('LINEBELOW', (0, -1), (-1, -1), 0.4, LINE),
    ]))
    return table


def notice(text: str) -> Table:
    table = Table([[para(text, 'SmallCS')]], colWidths=[WIDTH])
    table.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, -1), PALE),
        ('BOX', (0, 0), (-1, -1), 0.5, LINE),
        ('LEFTPADDING', (0, 0), (-1, -1), 11),
        ('RIGHTPADDING', (0, 0), (-1, -1), 11),
        ('TOPPADDING', (0, 0), (-1, -1), 10),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 7),
    ]))
    return table


class NumberedCanvas(canvas.Canvas):
    def __init__(self, *args, date: str, draft: bool, **kwargs):
        kwargs['invariant'] = 1
        super().__init__(*args, **kwargs)
        self.saved_pages = []
        self.state_date = clean(date)
        self.draft = draft
        self.page_count = 0

    def showPage(self):  # noqa: N802 - ReportLab API
        self.saved_pages.append(dict(self.__dict__))
        self._startPage()

    def save(self):
        count = len(self.saved_pages)
        for state in self.saved_pages:
            self.__dict__.update(state)
            self.setStrokeColor(LINE)
            self.setLineWidth(0.5)
            self.line(MARGIN, PAGE_H - 32, PAGE_W - MARGIN, PAGE_H - 32)
            self.line(MARGIN, 35, PAGE_W - MARGIN, 35)
            self.setFont('CS80Bold', 7.5)
            self.setFillColor(BLUE)
            self.drawString(MARGIN, PAGE_H - 24, 'CS-80 REFERENCE / COMPLETION AND HANDOVER')
            self.setFont('CS80Body', 7.5)
            self.setFillColor(MUTED)
            self.drawRightString(PAGE_W - MARGIN, PAGE_H - 24, self.state_date)
            self.drawString(MARGIN, 22, 'AGPLv3 | Public-source reconstruction | No hardware measurements')
            self.drawRightString(PAGE_W - MARGIN, 22, f'{self._pageNumber} / {count}')
            if self.draft:
                self.setFont('CS80Bold', 7.4)
                self.setFillColor(AMBER)
                self.drawRightString(PAGE_W - MARGIN, 46, 'WORKING DRAFT - RESOLVE PENDING FIELDS')
            super().showPage()
        super().save()
        self.page_count = count


def heading(number: int, title: str, subtitle: str) -> list[Flowable]:
    return [para(f'HANDOVER / {number:02d}', 'EyebrowCS'),
            para(title, 'H1CS'), para(subtitle, 'SmallCS'), Spacer(1, 6)]


def build_story(data: dict) -> list[Flowable]:
    story: list[Flowable] = []
    p = data['provenance']
    story += heading(1, 'CS-80 Reference', data['subtitle'])
    story.append(para(data['repository'] + ' | C++20 / JUCE 8.0.11 | AGPLv3', 'SmallCS'))
    story.append(Spacer(1, 5))
    for text in data['assessment']:
        story.append(para(text))
    story.append(para('Recorded source and landing', 'H2CS'))
    rows = [
        ['Production audio', p['source_sha']],
        ['Circuit V2 baseline', p['v2_pin']],
        ['Main merge', p['merge_sha']],
        ['Landing PR', para(linked('Pull request and review history', p['pr_url']), 'TableCS', raw=True)],
        ['Release source', p['release_source_sha']],
        ['Release result', para(markup(p['release_status']) + '<br/>' +
            linked('Installer workflow and artifacts', p['release_url']), 'TableCS', raw=True)],
    ]
    story.append(grid(['Record', 'Exact identity / result'], rows, [.25, .75]))
    story.append(para('Required workflow evidence', 'H2CS'))
    rows = [[para(linked(c['label'], c['url']), 'TableCS', raw=True),
             c['status'], c['sha']] for c in p['ci']]
    story.append(grid(['Workflow', 'Result', 'Tested source'], rows, [.25, .18, .57]))
    story.append(Spacer(1, 10))
    story.append(notice('Read docs/STATUS.md first for the current model register and open issues. '
        'This document is a dated snapshot. A successful validator or software reference gate '
        'does not establish a hardware match or replace the owner\'s real-DAW sessions.'))

    story.append(PageBreak())
    story += heading(2, 'Scorecard and measurement scope',
        'Before and after values retain their source, rate and measurement context. All acceptance limits stay unchanged.')
    rows = [[r[k] for k in ('metric', 'before', 'after', 'target', 'state')]
            for r in data['scorecard']]
    story.append(grid(['Metric / rate', 'Before / baseline', 'Qualified after', 'Target', 'State'],
                      rows, [.19, .22, .26, .19, .14]))
    story.append(Spacer(1, 10))
    for text in data['measurement_notes']:
        story.append(para(text, 'SmallCS'))

    story.append(PageBreak())
    story += heading(3, 'The six completion workstreams',
        'Implementation status and acceptance status are recorded separately.')
    if len(data['workstreams']) != 6:
        raise ValueError('Supply exactly six workstreams in the requested handover order.')
    for item in data['workstreams']:
        elements = [para(item['title'], 'H2CS'),
                    para(item['status'], 'EyebrowCS'), para(item['text'], 'SmallCS')]
        if item.get('commits'):
            elements.append(para('Published changes: ' + item['commits'], 'SmallCS'))
        story.append(KeepTogether(elements))

    story.append(PageBreak())
    story += heading(4, 'Evidence and compatibility',
        'The immutable audio boundaries and the causal proof remain part of the deliverable.')
    rows = [[r[k] for k in ('capture', 'baseline', 'result')]
            for r in data['compatibility']]
    story.append(grid(['Capture', 'Immutable baseline / current V2 pin', 'Result'], rows, [.24, .56, .20]))
    story.append(para('Intentional circuit-audio chronology', 'H2CS'))
    for item in data['audio_sequence']:
        story.append(para(item['change'], 'SmallCS'))
        story.append(para('Audio: ' + item['audio'] + '\nSeparate V2 pin: ' + item['pin'] +
                          '\nStrict compatibility / status evidence: ' + item['evidence'], 'SmallCS'))
    story.append(para('Why the evidence supports the claims', 'H2CS'))
    for text in data['evidence_notes']:
        story.append(para(text, 'SmallCS'))
    story.append(Spacer(1, 4))
    story.append(notice('Future changes: commit tolerances before evaluating; mutation-check every new gate. '
        'Push a circuit-audio commit, make its separate full-SHA V2 pin commit, then run '
        'scripts/check-compat-gates.sh --strict-v2 and record the result in STATUS. '
        'Never weaken, skip or delete a gate to obtain a pass.'))

    story.append(PageBreak())
    story += heading(5, 'Remaining work and owner boundaries',
        'An unresolved target stays open, with the evidence needed for the next decision.')
    for item in data['remaining']:
        story.append(KeepTogether([para(item['title'], 'H2CS'), para(item['text'])]))
    story.append(para('Owner-only actions - outside this task', 'H2CS'))
    for item in data['owner_only']:
        story.append(para('- ' + item))
    story.append(Spacer(1, 8))
    story.append(notice('Evidence rule: cite the schematic/manual page for each physical law, '
        'or label it a declared inference and retain its alternatives. The HP setup discrepancy '
        'and ambiguous tone-cell revisions do not justify curve fitting. No original instrument '
        'was measured in this work.'))

    story.append(PageBreak())
    story += heading(6, 'Reproduce and continue',
        'Use the exact source identities above. Run the required checks after each relevant change.')
    lines = []
    for command in data['commands']:
        wrapped = textwrap.wrap(clean(command), width=103, break_long_words=False,
                                break_on_hyphens=False, subsequent_indent='  ')
        lines.extend([line + (' \\' if index < len(wrapped) - 1 else '')
                      for index, line in enumerate(wrapped)])
        lines.append('')
    code = Preformatted('\n'.join(lines).rstrip(), STYLES['CodeCS'])
    box = Table([[code]], colWidths=[WIDTH])
    box.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, -1), PALE),
        ('BOX', (0, 0), (-1, -1), .5, LINE),
        ('LEFTPADDING', (0, 0), (-1, -1), 10),
        ('RIGHTPADDING', (0, 0), (-1, -1), 10),
        ('TOPPADDING', (0, 0), (-1, -1), 10),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 10),
    ]))
    story.append(box)
    story.append(Spacer(1, 8))
    story.append(para('UI smoke requires Xvfb, xauth and openbox. An isolated Openbox exit 70 '
        'is an environmental display failure; retain its log and rerun the unchanged check. '
        'CPU comparisons require at least three rounds per source and serial measurement '
        'after builds finish.', 'SmallCS'))
    story.append(para('Primary references and retained evidence', 'H2CS'))
    for item in data['references']:
        story.append(para(linked(item['label'], item['url']), 'SmallCS', raw=True))
    story.append(Spacer(1, 6))
    story.append(para('Continuation rules', 'H2CS'))
    story.append(para('Keep parameter IDs and ordering append-only, use enum names, and select '
        'the next unused AU version hint (16 or higher). Keep modern additions neutral by '
        'default and inert in isPureCs80(policy). Before copying or replacing APVTS state, '
        'take getUndoableState().treeLock() and call syncNow(). Use small reviewable commits; '
        'never force-push or rewrite published history.', 'SmallCS'))
    return story


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--data', required=True, type=Path, help='UTF-8 JSON evidence data')
    parser.add_argument('--out', type=Path, default=Path(__file__).resolve().parents[1] /
                        'docs/CS80_PLAN_COMPLETION_AND_HANDOVER.pdf')
    parser.add_argument('--draft', action='store_true', help='Allow pending placeholders and label every page')
    args = parser.parse_args()
    data = json.loads(args.data.read_text(encoding='utf-8'))
    if not args.draft and re.search(r'\b(?:PENDING|TODO)(?:\b|_)|\[FINAL:', json.dumps(data, ensure_ascii=False)):
        parser.error('Unresolved placeholder in evidence data; finalize it or explicitly use --draft.')
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(prefix='.cs80-handover-', suffix='.pdf',
                                     dir=args.out.parent, delete=False) as handle:
        temporary = Path(handle.name)
    try:
        doc = SimpleDocTemplate(str(temporary), pagesize=A4, leftMargin=MARGIN,
            rightMargin=MARGIN, topMargin=51, bottomMargin=62,
            title='CS-80 Reference: completion and handover', author=data['repository'],
            subject='Source, qualification, open acceptance boundaries and release handover',
            pageCompression=1)
        doc.build(build_story(data), canvasmaker=lambda *a, **kw: NumberedCanvas(
            *a, date=data['date'], draft=args.draft, **kw))
        if doc.canv.page_count != PAGE_COUNT:
            raise ValueError(f'Content overflow: rendered {doc.canv.page_count} pages; expected '
                f'{PAGE_COUNT}. Shorten the overflowing section rather than shrinking the type.')
        temporary.replace(args.out)
        args.out.chmod(0o644)
    finally:
        temporary.unlink(missing_ok=True)
    print(f'Wrote {PAGE_COUNT} A4 pages: {args.out}')


if __name__ == '__main__':
    main()
