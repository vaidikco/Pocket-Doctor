import unittest

class Profile:
    def __init__(self, name="", age=30, height=170, weight=70, intox="None", history=False, treatment="Allopathy"):
        self.name = name
        self.age = age
        self.height = height
        self.weight = weight
        self.intox = intox
        self.history = history
        self.treatment = treatment

def validate_profile(p, current_count, is_new):
    if len(p.name.strip()) == 0:
        return False, "Name cannot be empty"
    if p.age < 1 or p.age > 120:
        return False, "Age out of range"
    if p.height < 50 or p.height > 250:
        return False, "Height out of range"
    if p.weight < 20 or p.weight > 300:
        return False, "Weight out of range"
    if is_new and current_count >= 3:
        return False, "Maximum 3 profiles reached"
    return True, "Valid"

class TestProfileValidation(unittest.TestCase):
    def test_valid_profile(self):
        p = Profile("Vaidik", 25, 175, 72, "None", False, "Allopathy")
        valid, msg = validate_profile(p, 0, True)
        self.assertTrue(valid)
        self.assertEqual(msg, "Valid")

    def test_empty_name(self):
        p = Profile("", 25, 175, 72)
        valid, msg = validate_profile(p, 0, True)
        self.assertFalse(valid)
        self.assertIn("Name cannot be empty", msg)

    def test_age_out_of_bounds(self):
        p = Profile("Vaidik", 150, 175, 72)
        valid, msg = validate_profile(p, 0, True)
        self.assertFalse(valid)
        self.assertIn("Age out of range", msg)

    def test_max_profile_limit(self):
        p = Profile("New User", 30, 170, 70)
        valid, msg = validate_profile(p, 3, True)
        self.assertFalse(valid)
        self.assertIn("Maximum 3 profiles reached", msg)

if __name__ == "__main__":
    unittest.main()
