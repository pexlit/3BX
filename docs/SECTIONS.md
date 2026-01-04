possibilities for hardcoded sections:

# Named class with optional patterns/members
class <pattern>:
	patterns:
		<pattern>
		<pattern>
	members: m1, m2

# Anonymous class with pattern aliases (patterns: required)
class:
	patterns:
		<pattern>
		<pattern>
	members: m1, m2

expression <pattern>:
	patterns:
		<pattern>
		<pattern>
	get:
		<any code>
	set to $:
		<any code>

effect <pattern>:
	execute:
		<any code>
	patterns:
		<pattern>
		<pattern>
section <pattern>:
	execute:
		<any code>
	patterns:
		<pattern>
		<pattern>
<section pattern reference>:
	<any code>