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

class modbuf;
class codeinstr;

class obfengine
{
public:
	obfengine();
	~obfengine();

	void obfuscate_instr(codeinstr* cinstr, std::vector<codeinstr*> *obf_instr_vec);

	void set_obf_steps(u32 min_steps, u32 max_steps);

protected:
	u32 m_min_obf_steps;
	u32 m_max_obf_steps;

	enum calc_instr {
		CALC_ADD = 0,
		CALC_SUB,
		CALC_XOR,
		CALC_TYPE_MAX,
	};
	
	enum reg_type {
		R_EAX,
		R_ECX,
		R_EDX,
		R_EBX,
		R_ESP,
		R_EBP,
		R_ESI,
		R_EDI,
		MAX_REG_TYPE,
	};

	void obf_mov_reg_imm(codeinstr* cinstr, std::vector<codeinstr*> *obf_instr_vec);
	void obf_push_imm(codeinstr* cinstr, std::vector<codeinstr*> *obf_instr_vec);
	void obf_mov_reg_mem(codeinstr* cinstr, std::vector<codeinstr*> *obf_instr_vec);
	void obf_calc_reg(codeinstr* cinstr, std::vector<codeinstr*> *obf_instr_vec);
	void obf_mov_mem_imm(codeinstr* cinstr, std::vector<codeinstr*> *obf_instr_vec);

	void _gen_reg_calc(u8 reg, s32 imm, u32 steps, u8 nbytes, std::vector<codeinstr*> *obf_instr_vec);
	codeinstr* _gen_reg_calc_instr(u8 reg, s32 imm, u32 calc_type, u8 nbytes);

	codeinstr* _asm_mov_reg_imm(u8 reg, s32 imm, u8 nbytes);
	codeinstr* _asm_push_reg(u8 reg);
	codeinstr* _asm_mov_mreg_disp_reg(u8 reg_dst, s32 disp, u8 reg_src);
	codeinstr* _asm_pop_reg(u8 reg);
	codeinstr* _asm_mov_reg_reg_disp(u8 reg_dst, u8 reg_src, u8 reg_disp, u8 reg_bytes, u8 mov_dir);
	codeinstr* _asm_lea_reg_sib_reg(u8 reg_dst, u8 sib_base, u8 sib_index, u8 sib_scale);

	u8 _get_rand_reg(std::vector<u8> exclude_regs);

	void _add_instr_prefixes(codeinstr* new_cinstr, codeinstr* orig_cinstr);
	codeinstr* _create_instr32(const void *data, u32 label, u32 jmp_to_label);
	modbuf* _copy_buf(modbuf* buf);
};
