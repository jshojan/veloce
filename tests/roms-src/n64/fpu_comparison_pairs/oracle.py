from pathlib import Path
import importlib.util
spec=importlib.util.spec_from_file_location('comparison_oracle',Path(__file__).parent.parent/'fpu_comparison_edges/oracle.py')
base=importlib.util.module_from_spec(spec);spec.loader.exec_module(base)
expected=base.expected
