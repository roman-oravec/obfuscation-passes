import angr
import sys
import claripy

def solve(elf_binary="./binary.elf"):
  project = angr.Project(elf_binary) #load up binary
  arg = claripy.BVS('arg',8*0x20) #set a bit vector for argv[1]
  
  """   cfg = project.analyses.CFGEmulated(keep_state=True ,state_add_options=angr.sim_options.refs, context_sensitivity_level=2)
  cdg = project.analyses.CDG(cfg)
  ddg = project.analyses.DDG(cfg)
  target_func = cfg.kb.functions.function(name="main")

  target_node = cfg.get_any_node(target_func.addr)
  bs = project.analyses.BackwardSlice(cfg, cdg=cdg, ddg=ddg, targets=[ (target_node, -1) ])
  print(bs) """
  

  initial_state = project.factory.entry_state(args=[elf_binary,arg]) #set entry state for execution
  simulation = project.factory.simgr(initial_state) #get a simulation manager object under entry state
  simulation.explore(find=is_successful) #confine search path using our "Correct" criteria defined below

  if len(simulation.found) > 0: #if we find that our 'found' array is not empty then we have a solution!
    for solution_state in simulation.found:	#loop over each solution just for interest sake
      print("{!r}".format(solution_state.solver.eval(arg,cast_to=bytes))) #print the goodness!
  
  

def is_successful(state):
	output = state.posix.dumps(sys.stdout.fileno()) #grab the screen output everytime Angr thinks we have a solution
	if b'Correct' in output: 
		return True
	return False

if __name__=="__main__":
	if len(sys.argv) < 2:
		print("[*] need 2 arguments\nUsage: %s [binary path] [target address]")

	solve(sys.argv[1]) #solve!
