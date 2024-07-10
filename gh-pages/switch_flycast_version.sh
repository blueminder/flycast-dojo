#!/bin/bash
cd "$HOME/.fightcade2/emulator/flycast/"
if test -f flycast.elf.orig; then
	rm flycast.elf
	mv flycast.elf.orig flycast.elf
	echo "Bundled Fightcade Flycast Dojo Version Restored."
else
	mv flycast.elf flycast.elf.orig
	ln -s /usr/bin/flycast-dojo flycast.elf
	echo "Fightcade Flycast Dojo Version Set to System Installation."
fi
echo ""
read -rsn1 -p"Press any key to continue";echo

