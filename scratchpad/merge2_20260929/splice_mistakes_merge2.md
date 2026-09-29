- 12:24 (09-29): ran IDENT2's coverage.py on the merged bake with its built-in eyes and got 0.5176 against the lane's
  0.5877. The built-in eyes are IDENT1's older points; the lane passed its own three with `--eyes`. Caught because the
  lane's output file named different eye positions. Rerun with the lane's eyes: 0.5877, equal. Rule (skill
  nifskope-ww-campaign-merge, section 1, already written down): read the lane's exact arguments before comparing a
  number; the eye list goes into the chain script, not into a rerun.
