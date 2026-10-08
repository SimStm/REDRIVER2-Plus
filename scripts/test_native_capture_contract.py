"""Host checks for capture count scopes; actual GPU replay is tested separately."""

import ast
from pathlib import Path
import unittest


# Load only the pure contract: the inspector's top-level code requires an open
# RenderDoc controller. Actual replays verify its report integration separately.
source = Path(__file__).with_name("inspect_native_capture.py")
tree = ast.parse(source.read_text(encoding="utf-8-sig"), filename=str(source))
function = next(node for node in tree.body
                if isinstance(node, ast.FunctionDef) and node.name == "region_count_contract")
namespace = {}
exec(compile(ast.Module(body=[function], type_ignores=[]), str(source), "exec"), namespace)
contract = namespace["region_count_contract"]


class RegionCountContractTests(unittest.TestCase):
    def test_retained_auxiliary_batches_do_not_change_static_inventory(self):
        for auxiliary in (10, 11, 14):
            with self.subTest(auxiliary=auxiliary):
                result = contract({"backdrop": 80, "logicalWorld": 876,
                                   "auxiliary": auxiliary}, True)
                self.assertTrue(result["staticRegionCountsMatch"])
                self.assertEqual(result["auxiliaryBaselineCountsMatch"], auxiliary == 10)
                self.assertEqual(result["regionCountsMatch"], auxiliary == 10)
                self.assertEqual(result["expectedRegionCounts"]["auxiliary"], 10)

    def test_missing_or_extra_static_content_stays_a_failure(self):
        for backdrop, world in ((79, 876), (81, 876), (80, 875), (80, 877)):
            with self.subTest(backdrop=backdrop, world=world):
                result = contract({"backdrop": backdrop, "logicalWorld": world,
                                   "auxiliary": 10}, True)
                self.assertFalse(result["staticRegionCountsMatch"])
                self.assertTrue(result["auxiliaryBaselineCountsMatch"])
                self.assertFalse(result["regionCountsMatch"])

    def test_counts_cannot_hide_an_incomplete_index_partition(self):
        result = contract({"backdrop": 80, "logicalWorld": 876, "auxiliary": 10}, False)
        self.assertFalse(result["staticRegionCountsMatch"])
        self.assertFalse(result["regionCountsMatch"])


if __name__ == "__main__":
    unittest.main()
