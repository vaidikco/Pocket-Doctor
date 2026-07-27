# 🛠️ PocketDoctor Mark 4 — Test Suite Changelog Toolkit

This toolkit manages automated changelogs and release documentation for the **PocketDoctor Mark 4** test suite and web UI templates.

---

## 🚀 Toolkit CLI Usage

Run the `changelog_manager.py` Python script from the root or `tests/` directory:

### 1. Export Markdown Changelog (`tests/CHANGELOG_BETA.md`)
```bash
python tests/toolkit/changelog_manager.py export
```

### 2. List All Test Changelog Entries
```bash
python tests/toolkit/changelog_manager.py list
```

### 3. Search Changelog History
```bash
python tests/toolkit/changelog_manager.py search "Prescription"
```

### 4. Add a New Version Release Entry
```bash
python tests/toolkit/changelog_manager.py add --version v1.2.0-beta --title "Added EEPROM Stress Test" --changes "Added test_eeprom_write.py" "Updated test runner"
```

---

## 📁 File Structure
- `changelog_manager.py`: Core CLI manager script for searching, adding, and rendering markdown changelogs.
- `test_changelog_history.json`: Structured JSON repository of all test suite release logs.
- `README.md`: Toolkit usage documentation.
