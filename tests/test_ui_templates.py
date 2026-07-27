import unittest
import re
import os

MAIN_INO_PATH = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "src", "main.ino")

class TestUITemplates(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with open(MAIN_INO_PATH, "r", encoding="utf-8") as f:
            cls.content = f.read()

    def test_dashboard_template_placeholders(self):
        """Verify DASHBOARD_HTML template contains required %IP% and %UPTIME% placeholders."""
        self.assertIn("DASHBOARD_HTML", self.content)
        match = re.search(r'DASHBOARD_HTML\[\]\s*PROGMEM\s*=\s*R"rawliteral\((.*?)\)rawliteral";', self.content, re.DOTALL)
        self.assertIsNotNone(match, "DASHBOARD_HTML template block not found")
        tmpl = match.group(1)
        self.assertIn("%IP%", tmpl, "Missing %IP% placeholder in DASHBOARD_HTML")
        self.assertIn("%UPTIME%", tmpl, "Missing %UPTIME% placeholder in DASHBOARD_HTML")

    def test_wifi_template_placeholders(self):
        """Verify WIFI_HTML template contains required %SSID% and %PASS% placeholders."""
        self.assertIn("WIFI_HTML", self.content)
        match = re.search(r'WIFI_HTML\[\]\s*PROGMEM\s*=\s*R"rawliteral\((.*?)\)rawliteral";', self.content, re.DOTALL)
        self.assertIsNotNone(match, "WIFI_HTML template block not found")
        tmpl = match.group(1)
        self.assertIn("%SSID%", tmpl, "Missing %SSID% placeholder in WIFI_HTML")
        self.assertIn("%PASS%", tmpl, "Missing %PASS% placeholder in WIFI_HTML")

    def test_profiles_template_placeholders(self):
        """Verify PROFILES_HTML template contains required %PROFILES% placeholder."""
        self.assertIn("PROFILES_HTML", self.content)
        match = re.search(r'PROFILES_HTML\[\]\s*PROGMEM\s*=\s*R"rawliteral\((.*?)\)rawliteral";', self.content, re.DOTALL)
        self.assertIsNotNone(match, "PROFILES_HTML template block not found")
        tmpl = match.group(1)
        self.assertIn("%PROFILES%", tmpl, "Missing %PROFILES% placeholder in PROFILES_HTML")

    def test_profile_edit_template_placeholders(self):
        """Verify PROFILE_EDIT_HTML template contains all required form placeholders."""
        match = re.search(r'PROFILE_EDIT_HTML\[\]\s*PROGMEM\s*=\s*R"rawliteral\((.*?)\)rawliteral";', self.content, re.DOTALL)
        self.assertIsNotNone(match, "PROFILE_EDIT_HTML template block not found")
        tmpl = match.group(1)
        required_placeholders = [
            "%HEADER%", "%INDEX%", "%NAME%", "%AGE%", "%HEIGHT%", "%WEIGHT%",
            "%SEL_NONE%", "%SEL_ALCOHOL%", "%SEL_TOBACCO%", "%SEL_BOTH%",
            "%SEL_HIST_YES%", "%SEL_HIST_NO%", "%SEL_ALLOPATHY%",
            "%SEL_HOMEOPATHY%", "%SEL_AYURVEDIC%", "%SEL_ALLTYPES%"
        ]
        for ph in required_placeholders:
            self.assertIn(ph, tmpl, f"Missing {ph} placeholder in PROFILE_EDIT_HTML")

    def test_report_generation_function(self):
        """Verify createComprehensiveHTMLReport function exists and produces HTML structure."""
        self.assertIn("String createComprehensiveHTMLReport()", self.content)
        self.assertIn("diagnosedDisease", self.content)
        self.assertIn("currentProfile.name", self.content)
        self.assertIn("createComprehensiveHTMLReport", self.content)

    def test_modern_css_features(self):
        """Verify modern CSS features like CSS variables, flex/grid, and media queries exist."""
        self.assertIn(":root", self.content)
        self.assertIn("grid-template-columns", self.content)
        self.assertIn("@media", self.content)

if __name__ == "__main__":
    unittest.main()
