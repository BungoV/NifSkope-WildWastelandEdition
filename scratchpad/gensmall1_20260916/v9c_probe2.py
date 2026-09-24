import sys, importlib.util
spec = importlib.util.spec_from_file_location('p', sys.argv[0].replace('probe2','probe').replace('.py','.py'))
