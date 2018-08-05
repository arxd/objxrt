# Main SCons file

debug = int(ARGUMENTS.get('debug',3))

env = Environment(tools=['default',TOOL_HDRTMPL, TOOL_SHADER])

if debug <= 1:
	env.Append(CCFLAGS = '-O3')
else:
	env.Append(CCFLAGS = '-g')
env.Append(CCFLAGS = '-DLOG_LEVEL=%d'%debug)

SConscript('src/SConstruct', variant_dir='build/', exports='env' )

Clean('.', 'build')


