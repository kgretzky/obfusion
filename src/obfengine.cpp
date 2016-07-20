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

#include "include.h"
#include "obfengine.h"
#include "codeinstr.h"
#include "modbuf.h"
#include "mt.h"

obfengine::obfengine()
{
	m_min_obf_steps = 3;
	m_max_obf_steps = 8;
}

obfengine::~obfengine()
{
}

void obfengine::obfuscate_instr(codeinstr* cinstr, std::vector<codeinstr*> *obf_instr_vec)
{
	u8 op1 = cinstr->get_hde32()->opcode;
	u8 op2 = cinstr->get_hde32()->opcode2;
	u8 modrm_mod = cinstr->get_hde32()->modrm_mod;

	if ( op1 >= 0xb0 && op1 <= 0xbf ) // mov reg, imm8/32
		obf_mov_reg_imm(cinstr, obf_instr_vec);
	else if ( op1 == 0x6a || op1 ==0x68 ) // push imm8/32
		obf_push_imm(cinstr, obf_instr_vec);
	else if ( op1 >= 0x88 && op1 <= 0x8b ) // mov reg, [reg+disp], mov [reg+disp], reg
		obf_mov_reg_mem(cinstr, obf_instr_vec);
	else if ( op1 >= 0x80 && op1 <= 0x83 ) // <add, or, adc, sbb, and, sub, xor, cmp> reg, imm
		obf_calc_reg(cinstr, obf_instr_vec);
	else if ( op1 >= 0xc6 && op1 <= 0xc7 ) // mov [mem], imm8/32
		obf_mov_mem_imm(cinstr, obf_instr_vec);
}

void obfengine::set_obf_steps(u32 min_steps, u32 max_steps)
{
	if (max_steps <= min_steps)
		max_steps = min_steps+1;

	m_min_obf_steps = min_steps;
	m_max_obf_steps = max_steps;
}

//////////////////////////////////////////////////////////////////////////

// mov reg, imm8/32
void obfengine::obf_mov_reg_imm(codeinstr* cinstr, std::vector<codeinstr*> *obf_instr_vec)
{
	u8 op1 = cinstr->get_hde32()->opcode;
	u8 op2 = cinstr->get_hde32()->opcode2;
	u8 p_66 = cinstr->get_hde32()->p_66;

	u8 reg = 0;
	u8 nbytes = 0;
	s32 imm = cinstr->get_rel_imm();

	u32 steps = (mt::rand_u32() % (m_max_obf_steps - m_min_obf_steps)) + m_min_obf_steps;

	if ( op1 >= 0xb0 && op1 < 0xb8 ) // mov r8, imm8
	{
		reg = op1 - 0xb0;
		nbytes = 1;
	}
	else if ( p_66 && op1 >= 0xb8 && op1 < 0xc0 ) // mov r16, imm16
	{
		reg = op1 - 0xb8;
		nbytes = 2;
	}
	else if ( op1 >= 0xb8 && op1 < 0xc0 ) // mov r32, imm32
	{
		reg = op1 - 0xb8;
		nbytes = 4;
	}

	_gen_reg_calc(reg, imm, steps, nbytes, obf_instr_vec);
}

// push imm8/32
void obfengine::obf_push_imm(codeinstr* cinstr, std::vector<codeinstr*> *obf_instr_vec)
{
	u8 nbytes = 4;
	s32 imm = cinstr->get_rel_imm();

	std::vector<u8> exc_regs;
	exc_regs.push_back(R_ESP);
	exc_regs.push_back(R_EBP);

	u8 treg = _get_rand_reg(exc_regs);

	obf_instr_vec->push_back(_asm_push_reg(_get_rand_reg(std::vector<u8>())));
	obf_instr_vec->push_back(_asm_push_reg(treg));

	u32 steps = (mt::rand_u32() % (m_max_obf_steps - m_min_obf_steps)) + m_min_obf_steps;

	_gen_reg_calc(treg, imm, steps, nbytes, obf_instr_vec);

	codeinstr *minstr = _asm_mov_mreg_disp_reg(R_ESP, 4, treg);
	_add_instr_prefixes(minstr, cinstr);
	obf_instr_vec->push_back(minstr);

	obf_instr_vec->push_back(_asm_pop_reg(treg));
}

// mov reg, [reg+disp]
// mov [reg+disp], reg
void obfengine::obf_mov_reg_mem(codeinstr* cinstr, std::vector<codeinstr*> *obf_instr_vec)
{
	u8 nbytes = 4;
	u8 mov_dir = 0;
	s32 disp = cinstr->get_rel_disp();

	hde32s *hde32 = cinstr->get_hde32();

	if (hde32->modrm_mod == 0x03)
		return;

	u8 op1 = cinstr->get_hde32()->opcode;
	u8 reg_bytes;
	if (op1 == 0x8a || op1 == 0x88)
		reg_bytes = 1;
	else if (hde32->p_66 > 0)
		reg_bytes = 2;
	else
		reg_bytes = 4;

	if (op1 == 0x88 || op1 == 0x89)
		mov_dir = 1;

	u8 reg_dst = hde32->modrm_reg;
	u8 reg_src = 0xff;
	u8 reg_index = 0xff;
	u8 reg_scale = 0xff;

	if (hde32->sib > 0)
	{
		reg_src = hde32->sib_base;
		reg_index = hde32->sib_index;
		reg_scale = hde32->sib_scale;
		if (hde32->modrm_mod == 0x00 && reg_src != R_EBP)
			return;
		if (reg_src == R_EBP)
			reg_src = 0xff;
	}
	else
	{
		if (hde32->modrm_mod > 0x00)
			reg_src = hde32->modrm_rm;
		if (hde32->modrm_mod == 0x00 && reg_src != R_EBP)
			return;
	}

	std::vector<u8> exc_regs;
	exc_regs.push_back(R_ESP);
	exc_regs.push_back(R_EBP);
	exc_regs.push_back(reg_dst);
	if (reg_src != 0xff)
		exc_regs.push_back(reg_src);
	if (reg_index != 0xff)
		exc_regs.push_back(reg_index);

	u8 treg = _get_rand_reg(exc_regs);

	u32 steps = (mt::rand_u32() % (m_max_obf_steps - m_min_obf_steps)) + m_min_obf_steps;

	// fix ESP if ESP register is used as source
	if (reg_src == R_ESP)
		disp += 4;

	obf_instr_vec->push_back(_asm_push_reg(treg));
	_gen_reg_calc(treg, disp, steps, 4, obf_instr_vec);

	if (reg_index != 0xff && reg_scale != 0xff)
		obf_instr_vec->push_back(_asm_lea_reg_sib_reg(treg, treg, reg_index, reg_scale));

	codeinstr *minstr;
	if ( reg_src != 0xff )
		minstr = _asm_mov_reg_reg_disp(reg_dst, reg_src, treg, reg_bytes, mov_dir);
	else
		minstr = _asm_mov_reg_reg_disp(reg_dst, treg, 0xff, reg_bytes, mov_dir);
	_add_instr_prefixes(minstr, cinstr);

	obf_instr_vec->push_back(minstr);
	obf_instr_vec->push_back(_asm_pop_reg(treg));
}

void obfengine::obf_calc_reg(codeinstr* cinstr, std::vector<codeinstr*> *obf_instr_vec)
{
	s32 imm = cinstr->get_rel_imm();
	s32 disp = cinstr->get_rel_disp();
	hde32s *hde32 = cinstr->get_hde32();
	u8 op1 = hde32->opcode;

	u8 nbytes = 4;
/*
	if ((hde32->flags & F_IMM8) == F_IMM8)
		nbytes = 1;
	else if (hde32->p_66 > 0)
		nbytes = 2;
	else
		nbytes = 4;
*/

	std::vector<u8> exc_regs;
	exc_regs.push_back(R_ESP);
	exc_regs.push_back(R_EBP);
/*
	if ((hde32->flags & F_IMM8) == F_IMM8)
	{
		exc_regs.push_back(R_ESI);
		exc_regs.push_back(R_EDI);
	}
*/

	if ((hde32->flags & F_SIB) == F_SIB)
	{
		exc_regs.push_back(hde32->sib_base);
		exc_regs.push_back(hde32->sib_index);
	}
	else
		exc_regs.push_back(hde32->modrm_rm);

	u8 reg_src = 0xff;
	if (hde32->sib > 0)
		reg_src = hde32->sib_base;
	else
	{
		if (hde32->modrm_mod > 0x00)
			reg_src = hde32->modrm_rm;
	}
	if (reg_src == R_ESP)
		return;

	u8 treg = _get_rand_reg(exc_regs);
	u8 mode = hde32->modrm_reg;

	u32 steps = (mt::rand_u32() % (m_max_obf_steps - m_min_obf_steps)) + m_min_obf_steps;

	obf_instr_vec->push_back(_asm_push_reg(treg));
	_gen_reg_calc(treg, imm, steps, nbytes, obf_instr_vec);

	u8 nopcode = 0;
	switch (mode)
	{
	case 0x00: // add
		if (op1 == 0x80 || op1 == 0x82)
			nopcode = 0x00;
		else if (op1 == 0x81 || op1 == 0x83)
			nopcode = 0x01;
		break;
	case 0x01: // or
		if (op1 == 0x80 || op1 == 0x82)
			nopcode = 0x08;
		else if (op1 == 0x81 || op1 == 0x83)
			nopcode = 0x09;
		break;
	case 0x02: // adc
		if (op1 == 0x80 || op1 == 0x82)
			nopcode = 0x10;
		else if (op1 == 0x81 || op1 == 0x83)
			nopcode = 0x11;
		break;
	case 0x03: // sbb
		if (op1 == 0x80 || op1 == 0x82)
			nopcode = 0x18;
		else if (op1 == 0x81 || op1 == 0x83)
			nopcode = 0x19;
		break;
	case 0x04: // and
		if (op1 == 0x80 || op1 == 0x82)
			nopcode = 0x20;
		else if (op1 == 0x81 || op1 == 0x83)
			nopcode = 0x21;
		break;
	case 0x05: // sub
		if (op1 == 0x80 || op1 == 0x82)
			nopcode = 0x28;
		else if (op1 == 0x81 || op1 == 0x83)
			nopcode = 0x29;
		break;
	case 0x06: // xor
		if (op1 == 0x80 || op1 == 0x82)
			nopcode = 0x30;
		else if (op1 == 0x81 || op1 == 0x83)
			nopcode = 0x31;
		break;
	case 0x07: // cmp
		if (op1 == 0x80 || op1 == 0x82)
			nopcode = 0x38;
		else if (op1 == 0x81 || op1 == 0x83)
			nopcode = 0x39;
		break;
	}

	u8 nmodrm = (hde32->modrm & 0xc7) | (treg << 3);
	
	modbuf data(16);
	codeinstr *minstr;

	if (hde32->p_66)
		data.add<u8>(0x66);
	data.add<u8>(nopcode);
	data.add<u8>(nmodrm);
	if ((hde32->flags & F_SIB) == F_SIB)
		data.add<u8>(hde32->sib);
	if ((hde32->flags & (F_DISP8|F_DISP32)))
	{
		if (hde32->flags & F_DISP8)
			data.add<s8>((s8)disp);
		else
			data.add<s32>(disp);
	}

	minstr = _create_instr32(data.data(), 0, 0);
	_add_instr_prefixes(minstr, cinstr);

	obf_instr_vec->push_back(minstr);
	obf_instr_vec->push_back(_asm_pop_reg(treg));
}

void obfengine::obf_mov_mem_imm(codeinstr* cinstr, std::vector<codeinstr*> *obf_instr_vec)
{
	s32 imm = cinstr->get_rel_imm();
	s32 disp = cinstr->get_rel_disp();
	hde32s *hde32 = cinstr->get_hde32();
	u8 op1 = hde32->opcode;

	u8 nbytes = 4;

	std::vector<u8> exc_regs;
	exc_regs.push_back(R_ESP);
	exc_regs.push_back(R_EBP);

	if ((hde32->flags & F_SIB) == F_SIB)
	{
		exc_regs.push_back(hde32->sib_base);
		exc_regs.push_back(hde32->sib_index);
	}
	else
		exc_regs.push_back(hde32->modrm_rm);

	u8 reg_src = 0xff;
	if (hde32->sib > 0)
		reg_src = hde32->sib_base;
	else
	{
		if (hde32->modrm_mod > 0x00)
			reg_src = hde32->modrm_rm;
	}
	if (reg_src == R_ESP)
		return;

	u8 treg = _get_rand_reg(exc_regs);

	u32 steps = (mt::rand_u32() % (m_max_obf_steps - m_min_obf_steps)) + m_min_obf_steps;

	obf_instr_vec->push_back(_asm_push_reg(treg));
	_gen_reg_calc(treg, imm, steps, nbytes, obf_instr_vec);

	u8 nopcode = 0;
	if (op1 == 0xc7)
		nopcode = 0x89;
	else if (op1 == 0xc6)
		nopcode = 0x88;

	u8 nmodrm = (hde32->modrm & 0xc7) | (treg << 3);

	modbuf data(16);
	codeinstr *minstr;

	if (hde32->p_66)
		data.add<u8>(0x66);
	data.add<u8>(nopcode);
	data.add<u8>(nmodrm);
	if ((hde32->flags & F_SIB) == F_SIB)
		data.add<u8>(hde32->sib);
	if ((hde32->flags & (F_DISP8|F_DISP32)))
	{
		if (hde32->flags & F_DISP8)
			data.add<s8>((s8)disp);
		else
			data.add<s32>(disp);
	}

	minstr = _create_instr32(data.data(), 0, 0);
	_add_instr_prefixes(minstr, cinstr);

	obf_instr_vec->push_back(minstr);
	obf_instr_vec->push_back(_asm_pop_reg(treg));
}

void obfengine::_gen_reg_calc(u8 reg, s32 imm, u32 steps, u8 nbytes, std::vector<codeinstr*> *obf_instr_vec)
{
	s32 cimm = imm;

	std::vector<u32> calc_type_vec;
	std::vector<u8> reg_vec;
	std::vector<s32> rimm_vec;

	u32 mask = 0;
	switch (nbytes)
	{
	case 1:
		mask = 0xff;
		break;
	case 2:
		mask = 0xffff;
		break;
	case 4:
		mask = 0xffffffff;
		break;
	}

	for (u32 i=0; i<steps; ++i)
	{
		s32 rimm = mt::rand_u32() & mask;
		u32 calc_type = mt::rand_u32() % CALC_TYPE_MAX;
		switch (calc_type)
		{
		case CALC_ADD:
			cimm -= rimm;
			break;
		case CALC_SUB:
			cimm += rimm;
			break;
		case CALC_XOR:
			cimm ^= rimm;
			break;
		}

		calc_type_vec.push_back(calc_type);
		reg_vec.push_back(reg);
		rimm_vec.push_back(rimm);
	}

	obf_instr_vec->push_back(_asm_mov_reg_imm(reg, cimm, nbytes));

	for (s32 i=calc_type_vec.size()-1; i>=0; --i)
	{
		u32 calc_type = calc_type_vec[i];
		u8 reg = reg_vec[i];
		s32 rimm = rimm_vec[i];

		codeinstr *cinstr = _gen_reg_calc_instr(reg, rimm, calc_type, nbytes);
		if (cinstr)
			obf_instr_vec->push_back(cinstr);
	}
}

codeinstr* obfengine::_gen_reg_calc_instr(u8 reg, s32 imm, u32 calc_type, u8 nbytes)
{
	u8 op1 = 0x81;
	u8 modrm = 0;
	if (nbytes == 1)
		op1 = 0x82;

	modbuf data;

	switch (calc_type)
	{
	case CALC_ADD:
		modrm = 0xc0 + reg;
		break;
	case CALC_SUB:
		modrm = 0xe8 + reg;
		break;
	case CALC_XOR:
		modrm = 0xf0 + reg;
		break;
	}

	if (nbytes == 2)
		data.add<u8>(0x66);
	data.add<u8>(op1);
	data.add<u8>(modrm);
	switch (nbytes)
	{
	case 1:
		data.add<s8>((s8)imm);
		break;
	case 2:
		data.add<s16>((s16)imm);
		break;
	case 4:
		data.add<s32>(imm);
		break;
	}

	return _create_instr32(data.data(), 0, 0);
}

codeinstr* obfengine::_asm_mov_reg_imm(u8 reg, s32 imm, u8 nbytes)
{
	u8 op1 = 0xb0;
	if (nbytes > 1)
		op1 = 0xb8;

	modbuf data(16);

	if (nbytes == 2)
		data.add<u8>(0x66);
	data.add<u8>(op1 + reg);
	switch (nbytes)
	{
	case 1:
		data.add<s8>((s8)imm);
		break;
	case 2:
		data.add<s16>((s16)imm);
		break;
	case 4:
		data.add<s32>(imm);
		break;
	}

	return _create_instr32(data.data(), 0, 0);
}

codeinstr* obfengine::_asm_push_reg(u8 reg)
{
	u8 op1 = 0x50;
	modbuf data(16);
	data.add<u8>(op1 + reg);

	return _create_instr32(data.data(), 0, 0);
}

codeinstr* obfengine::_asm_pop_reg(u8 reg)
{
	u8 op1 = 0x58;
	modbuf data(16);
	data.add<u8>(op1 + reg);

	return _create_instr32(data.data(), 0, 0);
}

codeinstr* obfengine::_asm_mov_mreg_disp_reg(u8 reg_dst, s32 disp, u8 reg_src)
{
	u8 op1 = 0x89; // mov mem, reg
	u8 nbytes;
	u8 modrm;
	u8 sib;

	if ( -128 <= disp && disp <= 127 )
	{
		nbytes = 1;
		modrm = 0x01 << 6;
	}
	else
	{
		nbytes = 4;
		modrm = 0x02 << 6;
	}

	modrm |= (reg_src << 3) | 0x04;
	sib = (reg_dst << 3) | R_ESP;

	modbuf data(16);
	data.add<u8>(op1);
	data.add<u8>(modrm);
	data.add<u8>(sib);

	if (nbytes == 1)
		data.add<s8>((s8)disp);
	else
		data.add<s32>(disp);

	return _create_instr32(data.data(), 0, 0);
}

codeinstr* obfengine::_asm_mov_reg_reg_disp(u8 reg_dst, u8 reg_src, u8 reg_disp, u8 reg_bytes, u8 mov_dir)
{
	u8 op1;

	if (reg_bytes == 1)
		op1 = 0x8a;
	else
		op1 = 0x8b;

	if (mov_dir == 1)
		op1 -= 2;

	u8 modrm;
	u8 sib;
	if (reg_disp != 0xff)
	{
		if (reg_src == R_EBP)
		{
			u8 treg = reg_disp;
			reg_disp = reg_src;
			reg_src = treg;
		}

		modrm = (reg_dst << 3) | 0x04;
		sib = reg_src | (reg_disp << 3);
	}
	else
	{
		modrm = (reg_dst << 3) | reg_src;
		sib = 0xff;
	}

	modbuf data(16);
	if (reg_bytes == 2)
		data.add<u8>(0x66);
	data.add<u8>(op1);
	data.add<u8>(modrm);
	if (sib != 0xff)
		data.add<u8>(sib);

	return _create_instr32(data.data(), 0, 0);
}

codeinstr* obfengine::_asm_lea_reg_sib_reg(u8 reg_dst, u8 sib_base, u8 sib_index, u8 sib_scale)
{
	u8 op1 = 0x8d;
	u8 modrm = (reg_dst << 3) | 0x04;
	u8 sib = sib_base | (sib_index << 3) | (sib_scale << 6);

	modbuf data(16);
	data.add<u8>(op1);
	data.add<u8>(modrm);
	data.add<u8>(sib);

	return _create_instr32(data.data(), 0, 0);
}

u8 obfengine::_get_rand_reg(std::vector<u8> exclude_regs)
{
	u8 reg;

	do
	{
		reg = mt::rand_u32() % MAX_REG_TYPE;
	}
	while ( std::find(exclude_regs.begin(), exclude_regs.end(), reg) != exclude_regs.end() );

	return reg;
}

void obfengine::_add_instr_prefixes(codeinstr* new_cinstr, codeinstr* orig_cinstr)
{
	hde32s *hde32 = orig_cinstr->get_hde32();

	modbuf pdata(16);
	if (hde32->p_lock > 0)
		pdata.add<u8>(hde32->p_lock);
	if (hde32->p_rep > 0)
		pdata.add<u8>(hde32->p_rep);
	if (hde32->p_seg > 0)
		pdata.add<u8>(hde32->p_seg);

	pdata.add_data(new_cinstr->data(), new_cinstr->size());
	new_cinstr->set_data(pdata.data(), pdata.size());
}

codeinstr* obfengine::_create_instr32(const void *data, u32 label, u32 jmp_to_label)
{
	codeinstr *cinstr = codeinstr::disasm32(reinterpret_cast<const void*>(data));
	if (cinstr)
	{
		cinstr->set_label(label);
		cinstr->set_jmp_to_label(jmp_to_label);
		return cinstr;
	}
	return 0;
}

modbuf* obfengine::_copy_buf(modbuf* buf)
{
	modbuf *ret = new modbuf(16);
	ret->add_data(buf->data(), buf->size());

	return ret;
}