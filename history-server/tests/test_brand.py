from pathlib import Path
from urllib.parse import urljoin


def test_brand_matches_firmware():
    root = Path(__file__).resolve().parents[2]
    firmware = (root / "Backload/DashboardPage.h").read_text()
    archive = (root / "history-server/app/static/index.html").read_text()
    def brand(html):
        start = html.index('<div class="nav-brand">')
        return html[start:html.index('</div>', start) + len('</div>')]
    assert brand(archive) == brand(firmware)


def test_static_assets_and_csv_stay_under_the_archive_prefix():
    archive = (Path(__file__).resolve().parents[1] / "app/static/index.html").read_text()
    base = "https://testserver/historie/"
    for url in ("static/styles.css", "static/vendor/chart.umd.min.js", "static/app.js", "api/export.csv"):
        attribute = "src" if url.endswith(".js") else "href"
        assert f'{attribute}="{url}"' in archive
        assert urljoin(base, url) == base + url
