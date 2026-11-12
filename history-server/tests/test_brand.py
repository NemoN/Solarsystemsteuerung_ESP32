from pathlib import Path


def test_brand_matches_firmware():
    root = Path(__file__).resolve().parents[2]
    firmware = (root / "Backload/DashboardPage.h").read_text()
    archive = (root / "history-server/app/static/index.html").read_text()
    def brand(html):
        start = html.index('<div class="nav-brand">')
        return html[start:html.index('</div>', start) + len('</div>')]
    assert brand(archive) == brand(firmware)
