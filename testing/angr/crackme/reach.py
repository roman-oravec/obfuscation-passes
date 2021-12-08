import angr
import sys
import claripy


if __name__=="__main__":
  proj = angr.Project(sys.argv[1], auto_load_libs=False)
  cfg = proj.analyses.CFG(fail_fast = True)
  argv = [proj.filename]
  sym_arg = claripy.BVS('arg', 8*4)
  argv.append(sym_arg)
  state = proj.factory.entry_state(args=[argv])
  path_group = proj.factory.path_group(state)