/*

Obfusion - C++ X86 Code Obfuscation Library
Copyright (c) 2016 Kuba Udymowski-Grecki

Contact: kuba@breakdev.org

This file is part of Obfusion.

Obfusion is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Obfusion is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Obfusion.  If not, see <http://www.gnu.org/licenses/>.

*/

#pragma once

#include "../src/include.h"

#define OBFUSION_MAJOR_VER 0
#define OBFUSION_MINOR_VER 2

//////// Typedefs
typedef	unsigned char		u8;
typedef unsigned short		u16;
typedef unsigned int		u32;
typedef unsigned long long	u64;

typedef signed char			s8;
typedef signed short		s16;
typedef signed int			s32;
typedef signed long long	s64;

typedef float	f32;
typedef double	f64;

class codeinstr;
class modbuf;
class obfengine;

class obfusion
{
public:
	obfusion(void);
	virtual ~obfusion(void);

	void clean();
	void load32( const void* data, u32 size, std::vector<std::pair<u32,u32> > data_bytes = std::vector<std::pair<u32,u32> >() );

	bool obfuscate(u32 seed, u32 passes);

	void dump(unsigned char** data, u32 *size);

	void set_obf_steps(u32 min_steps, u32 max_steps);
	void set_jmp_perc(u32 jmp_perc);

	std::vector<codeinstr*> get_code() { return m_code; };
	u32 get_major_ver() { return OBFUSION_MAJOR_VER; };
	u32 get_minor_ver() { return OBFUSION_MINOR_VER; };
protected:
	u32 m_jmp_perc;

	obfengine *m_obfengine;

	std::vector<codeinstr*> m_code;
	u32 m_cur_label;

	std::map<codeinstr*,codeinstr*> m_jmp_map;
	std::map<u32,codeinstr*> m_label_cache;

	void parse_code();
	bool recalc_jumps();
	void fix_code();

	void mangle_code();

	void update_jumps();
	void update_cache();
	void clear_cache();

	void insert_code( std::vector<codeinstr*> *code_vec, u32 ni, const void* data, u32 size, u32 label, u32 jmp_to_label );
	void insert_code( u32 ni, const void* data, u32 size, u32 label, u32 jmp_to_label );
	void replace_code( u32 ni, const void* data, u32 size, u32 label, u32 jmp_to_label );
	void delete_code( u32 ni );

	bool mod_jmp_instr(codeinstr* cinstr, s32 jmp_delta, modbuf* jmp_bytes);

	u32 get_offset_by_instr(codeinstr* cinstr);
	codeinstr* get_instr_by_label(u32 label);

	u32 get_code_ni_by_offset(u32 offset);
	u32 get_next_label();
};
