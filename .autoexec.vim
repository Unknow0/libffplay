let s:name = "libffplay"
if match(expand("%:e"), "[hc]$")>=0
	kZ
	0
	setlocal noai
	exec "normal O/*******************************************************************************\<Esc>"
	exec "normal o * This file is part of ".s:name.".\<Esc>"
	exec "normal o *\<Esc>"
	exec "normal o * ".s:name." is free software; you can redistribute it and/or modify\<Esc>"
	exec "normal o * it under the terms of the GNU Lesser General Public License as published by\<Esc>"
	exec "normal o * the Free Software Foundation; either version 3 of the License, or\<Esc>"
	exec "normal o * (at your option) any later version.\<Esc>"
	exec "normal o *\<Esc>"
	exec "normal o * ".s:name." is distributed in the hope that it will be useful,\<Esc>"
	exec "normal o * but WITHOUT ANY WARRANTY; without even the implied warranty of\<Esc>"
	exec "normal o * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\<Esc>"
	exec "normal o * GNU Lesser General Public License for more details.\<Esc>"
	exec "normal o *\<Esc>"
	exec "normal o * You should have received a copy of the GNU Lesser General Public License along\<Esc>"
	exec "normal o * with ".s:name."; if not, write to the Free Software Foundation, Inc.,\<Esc>"
	exec "normal o * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.\<Esc>"
	exec "normal o ******************************************************************************/\<Esc>"
	'Z
	set ai<
endif
