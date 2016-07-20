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
#include "../include/obfusion.h"
#include "codeinstr.h"
#include "obfengine.h"
#include "modbuf.h"
#include "mt.h"

#define DEBUG_PRINTF // printf

obfusion::obfusion(void)
{
	m_jmp_perc = 5;
	m_obfengine = new obfengine();

	clean();
}

obfusion::~obfusion(void)
{
	clean();
	delete m_obfengine;
}

void obfusion::clean()
{
	for ( std::vector<codeinstr*>::iterator it = m_code.begin(); it != m_code.end(); )
	{
		codeinstr *cinstr = (*it);
		if (cinstr)
			delete cinstr;
		it = m_code.erase(it);
	}

	m_cur_label = 1;
}

void obfusion::load32( const void* data, u32 size, std::vector<std::pair<u32,u32>> data_bytes )
{
	clean();

	const u8 *bdata = reinterpret_cast<const u8*>(data);
	u32 offset = 0;

	codeinstr *cinstr = 0;
	do
	{
		u32 data_size = 0;

		// check if we hit the data block
		for ( std::vector<std::pair<u32,u32>>::iterator it = data_bytes.begin(); it != data_bytes.end(); ++it )
		{
			std::pair<u32,u32> drange = (*it);
			if ( offset >= drange.first && offset < drange.second )
			{
				data_size = drange.second - offset;
				break;
			}
		}

		if (data_size == 0)
		{
			cinstr = codeinstr::disasm32(reinterpret_cast<const void*>(bdata + offset));
			if (cinstr)
			{
				offset += cinstr->size();
				m_code.push_back(cinstr);
			}
		}
		else
		{
			cinstr = codeinstr::data_block(reinterpret_cast<const void*>(bdata + offset), data_size);
			if (cinstr)
			{
				offset += cinstr->size();
				m_code.push_back(cinstr);
			}
		}

		if (offset >= size)
			break;
	}
	while ( cinstr );

	parse_code();
	fix_code();
}

bool obfusion::obfuscate(u32 seed, u32 passes)
{
	mt::seed(seed);

	for ( u32 pi=0; pi<passes; ++pi )
	{
		u32 ni = 0;
		for ( std::vector<codeinstr*>::iterator it = m_code.begin(); it != m_code.end(); ++it )
		{
			codeinstr *cinstr = (*it);
			if (cinstr)
			{
				if ( cinstr->is_data() == false )
				{
					std::vector<codeinstr*> obf_instr_vec;

					m_obfengine->obfuscate_instr(cinstr, &obf_instr_vec);
					if ( obf_instr_vec.size() > 0 )
					{
						bool first_instr = true;
						for ( std::vector<codeinstr*>::iterator mit = obf_instr_vec.begin(); mit != obf_instr_vec.end(); ++mit )
						{
							codeinstr *obf_instr = (*mit);
							if (obf_instr)
							{
								if ( first_instr )
								{
									replace_code(ni++, obf_instr->data(), obf_instr->size(), cinstr->label(), obf_instr->jmp_to_label());
									first_instr = false;
								}
								else
								{
									insert_code(ni++, obf_instr->data(), obf_instr->size(), obf_instr->label(), obf_instr->jmp_to_label());
								}
								delete obf_instr;
							}
						}
						--ni;
						it = m_code.begin()+ni;
					}
				}

				++ni;
			}
		}
	}

	mangle_code();

	return recalc_jumps();
}

void obfusion::dump(unsigned char** data, u32 *size)
{
	modbuf _data;
	for ( std::vector<codeinstr*>::iterator it = m_code.begin(); it != m_code.end(); ++it )
	{
		codeinstr *cinstr = (*it);
		if (cinstr)
		{
			_data.add_data(cinstr->data(), cinstr->size());
		}
	}

	if (_data.size() > 0)
	{
		*data = new u8[_data.size()];
		memcpy(*data, _data.data(), _data.size());
		*size = _data.size();
	}
}

void obfusion::set_jmp_perc(u32 jmp_perc)
{
	if (jmp_perc > 100)
		jmp_perc = 100;

	m_jmp_perc = jmp_perc;
}

void obfusion::set_obf_steps(u32 min_steps, u32 max_steps)
{
	m_obfengine->set_obf_steps(min_steps, max_steps);
}

void obfusion::insert_code( std::vector<codeinstr*> *code_vec, u32 ni, const void* data, u32 size, u32 label, u32 jmp_to_label )
{
	if ( ni > code_vec->size() )
		return;

	std::vector<codeinstr*>::iterator citor = code_vec->begin()+ni;
	const u8* bdata = reinterpret_cast<const u8*>(data);

	codeinstr *cinstr = 0;
	u32 offset = 0;
	u32 icnt = 0;
	do 
	{
		cinstr = codeinstr::disasm32(reinterpret_cast<const void*>(bdata + offset));
		if (cinstr)
		{
			// if new jump was created, update the jmp map
			// NOTE: make sure the instruction with proper label already exists before create a jump
			// if the label is not present at the time of insertion, make sure to call update_jumps() after this call
			if ( jmp_to_label > 0 )
			{
				codeinstr *jinstr;
				if ( m_label_cache.find(jmp_to_label) != m_label_cache.end() )
					jinstr = m_label_cache[jmp_to_label];
				else
					jinstr = get_instr_by_label(jmp_to_label);

				if (jinstr)
					m_jmp_map[cinstr] = jinstr;
			}

			cinstr->set_label(label);
			cinstr->set_jmp_to_label(jmp_to_label);

			offset += cinstr->size();
			code_vec->insert(citor, cinstr);
			++icnt;
			citor = code_vec->begin()+ni+icnt;
		}
	}
	while (cinstr && offset < size);
}

void obfusion::insert_code( u32 ni, const void* data, u32 size, u32 label, u32 jmp_to_label )
{
	insert_code( &m_code, ni, data, size, label, jmp_to_label );
}

void obfusion::replace_code( u32 ni, const void* data, u32 size, u32 label, u32 jmp_to_label )
{
	if ( ni >= m_code.size() )
		return;

	std::vector<codeinstr*>::iterator citor = m_code.begin()+ni;
	const u8* bdata = reinterpret_cast<const u8*>(data);

	codeinstr *cinstr = 0;
	u32 offset = 0;
	u32 icnt = 0;
	do 
	{
		cinstr = codeinstr::disasm32(reinterpret_cast<const void*>(bdata + offset));
		if (cinstr)
		{
			offset += cinstr->size();
			if (icnt == 0)
			{
				// remove reference in jmp map if jmp was present at replaced instruction and is removed
				if ( m_code[ni]->jmp_to_label() != 0 && m_code[ni]->jmp_to_label() != jmp_to_label )
				{
					std::map<codeinstr*,codeinstr*>::iterator jitor = m_jmp_map.find(m_code[ni]);
					if ( jitor != m_jmp_map.end() )
						m_jmp_map.erase(jitor);
				}
				// if new jump was created, update the jmp map
				if ( jmp_to_label > 0 && m_code[ni]->jmp_to_label() != jmp_to_label )
				{
					codeinstr *jinstr = get_instr_by_label(jmp_to_label);
					if (jinstr)
						m_jmp_map[m_code[ni]] = jinstr;
				}

				m_code[ni]->set_data(cinstr->data(), cinstr->size());
				m_code[ni]->set_label(label);
				m_code[ni]->set_jmp_to_label(jmp_to_label);
				delete cinstr;
			}
			else
				m_code.insert(citor, cinstr);

			++icnt;
			citor = m_code.begin()+ni+icnt;
		}
	}
	while (cinstr && offset < size);
}

void obfusion::delete_code( u32 ni )
{
	if ( ni >= m_code.size() )
		return;

	std::map<codeinstr*,codeinstr*>::iterator jitor_next;
	std::vector<codeinstr*>::iterator citor = m_code.begin()+ni;

	if ( m_code.size() > 1 )
	{
		std::vector<codeinstr*>::iterator citor_next;
		if ( m_code.begin()+ni+1 == m_code.end() )
			citor_next = m_code.begin()+ni-1;
		else
			citor_next = m_code.begin()+ni+1;

		std::map<codeinstr*,codeinstr*>::iterator jitor = m_jmp_map.find((*citor));
		if ( jitor != m_jmp_map.end() )
			m_jmp_map.erase(jitor);

		u32 cur_label = (*citor)->label();
		// merge two labels if next instruction has its own label
		u32 next_label = (*citor_next)->label();
		if ( next_label > 0 )
		{
			for ( jitor = m_jmp_map.begin(), jitor_next = jitor; jitor != m_jmp_map.end(); jitor=jitor_next )
			{
				++jitor_next;

				codeinstr *jmp_from = (*jitor).first;
				jmp_from->set_jmp_to_label(cur_label);
			}
		}
		// set next instruction's label to the label of deleted instruction
		(*citor_next)->set_label(cur_label);

		// update the jump map to reference to new jmp_to instruction
		for ( jitor = m_jmp_map.begin(), jitor_next = jitor; jitor != m_jmp_map.end(); jitor=jitor_next )
		{
			++jitor_next;

			codeinstr *jmp_from = (*jitor).first;
			codeinstr *jmp_to = (*jitor).second;

			if ( jmp_to == (*citor) )
			{
				(*jitor).second = (*citor_next);
			}
		}
	}

	m_code.erase(citor);
}

void obfusion::parse_code()
{
	u32 offset = 0;
	u32 ni = 0;

	for ( std::vector<codeinstr*>::iterator it = m_code.begin(); it != m_code.end(); ++it )
	{
		codeinstr *cinstr = (*it);
		if (cinstr)
		{
			if ( cinstr->is_data() == false && cinstr->is_rel_jmp() )
			{
				// relative jump detected
				// calculate the jump destination and set up labels

				s32 rel_imm = cinstr->get_rel_imm();
				u32 jmp_offset = offset + cinstr->size() + rel_imm;

				u32 jmp_ni = get_code_ni_by_offset(jmp_offset);
				if ( jmp_ni != -1 )
				{
					u32 jmp_to_label = m_code[jmp_ni]->label();
					if ( jmp_to_label == 0 )
						jmp_to_label = get_next_label();

					m_code[jmp_ni]->set_label(jmp_to_label);
					cinstr->set_jmp_to_label(jmp_to_label);

					m_jmp_map[cinstr] = m_code[jmp_ni];
				}
			}

			offset += cinstr->size();
			++ni;
		}
	}
}

bool obfusion::recalc_jumps()
{
	u32 diff_cnt = 0;

	DEBUG_PRINTF(__FUNCTION__": begin - m_jmp_map.size() = %u\n", m_jmp_map.size() );

	std::map<codeinstr*,u32> instr_map;
	for ( std::map<codeinstr*,codeinstr*>::iterator it = m_jmp_map.begin(); it != m_jmp_map.end(); ++it )
	{
		codeinstr *jmp_from = (*it).first;
		codeinstr *jmp_to = (*it).second;

		instr_map[jmp_from] = 0;
		instr_map[jmp_to] = 0;
	}
	u32 offset = 0;
	for ( std::vector<codeinstr*>::iterator it = m_code.begin(); it != m_code.end(); ++it )
	{
		codeinstr *cinstr = (*it);
		
		if ( instr_map.find(cinstr) != instr_map.end() )
			instr_map[cinstr] = offset;

		offset += cinstr->size();
	}

	DEBUG_PRINTF(__FUNCTION__": cache init done.\n" );

	std::map<u32,s32> sdiff_map;

	while (1)
	{
		diff_cnt = 0;
		for ( std::map<codeinstr*,codeinstr*>::iterator it = m_jmp_map.begin(); it != m_jmp_map.end(); ++it )
		{
			codeinstr *jmp_from = (*it).first;
			codeinstr *jmp_to = (*it).second;

			u32 jmp_from_offset = instr_map[jmp_from];
			u32 jmp_to_offset = instr_map[jmp_to];
			u32 orig_jmp_from_offset = jmp_from_offset;
			u32 orig_jmp_to_offset = jmp_to_offset;

			if ( sdiff_map.size() >= 0x200 )
			{
				u32 offset = 0;
				for ( std::vector<codeinstr*>::iterator it = m_code.begin(); it != m_code.end(); ++it )
				{
					codeinstr *cinstr = (*it);

					if ( instr_map.find(cinstr) != instr_map.end() )
						instr_map[cinstr] = offset;

					offset += cinstr->size();
				}
				sdiff_map.clear();
			}

/*
			OPTIMIZATION ATTEMPT #1 - somewhat failed, rare cases when bad offsets are produced
			
			for (std::map<u32,s32>::iterator sit = sdiff_map.begin(); sit != sdiff_map.end(); ++sit)
			{
				u32 offset = (*sit).first;
				if (offset < orig_jmp_from_offset)
					jmp_from_offset += (*sit).second;
				if (offset < orig_jmp_to_offset)
					jmp_to_offset += (*sit).second;
			}
*/

			s32 real_rel_jmp = jmp_to_offset - (jmp_from_offset + jmp_from->size());
			s32 cur_rel_jmp = jmp_from->get_rel_imm();

			if ( real_rel_jmp != cur_rel_jmp )
			{
				// jump offsets not equal - fix is required
				
				modbuf jmp_bytes(16);
				if ( mod_jmp_instr(jmp_from, real_rel_jmp, &jmp_bytes) )
				{
					s32 size_diff = jmp_bytes.size() - jmp_from->size();

/*
					OPTIMIZATION ATTEMPT #1 - somewhat failed, rare cases when bad offsets are produced
					if ( size_diff != 0 )
					{
						if ( sdiff_map.find(orig_jmp_from_offset) != sdiff_map.end() )
							sdiff_map[orig_jmp_from_offset] += size_diff;
						else
							sdiff_map[orig_jmp_from_offset] = size_diff;
					}
*/


					if ( size_diff != 0 )
					{
						for ( std::map<codeinstr*,u32>::iterator jit = instr_map.begin(); jit != instr_map.end(); ++jit )
						{
							u32 offset = (*jit).second;
							if (offset > jmp_from_offset)
								(*jit).second += size_diff;
						}
					}


					jmp_from->set_data(jmp_bytes.data(), jmp_bytes.size());
					++diff_cnt;
				}
				else
					return false;
			}
		}

		DEBUG_PRINTF(__FUNCTION__": diff_cnt = %u\n", diff_cnt);

		if (diff_cnt == 0)
			break;
	}
	DEBUG_PRINTF(__FUNCTION__": finish\n" );
	return true;
}

// replace problematic instructions
void obfusion::fix_code()
{
	u32 offset = 0;
	u32 ni = 0;

	for ( std::vector<codeinstr*>::iterator it = m_code.begin(); it != m_code.end(); ++it )
	{
		codeinstr *cinstr = (*it);
		if (cinstr)
		{
			if (cinstr->is_data() == false)
			{
				u8 op1 = cinstr->get_hde32()->opcode;
				u32 label = cinstr->label();
				u32 jmp_to_label = cinstr->jmp_to_label();

				if ( op1 == 0xe2 ) // loop
				{
					replace_code(ni++, "\x49", 1, label, 0); // dec ecx
					insert_code(ni++, "\x75\x00", 2, 0, jmp_to_label); // jnz
					--ni;
					it = m_code.begin()+ni;
				}
				else if ( op1 == 0xe3 ) // jecxz
				{
					replace_code(ni++, "\x85\xc9", 2, label, 0); // test ecx, ecx
					insert_code(ni++, "\x74\x00", 2, 0, jmp_to_label); // jz
					--ni;
					it = m_code.begin()+ni;
				}
				else if ( op1 == 0xe0 || op1 == 0xe1 )
				{
					u32 l1 = get_next_label();
					u32 l2 = get_next_label();

					if ( op1 == 0xe1 )
						replace_code(ni++, "\x74\x00", 2, label, l1);
					else
						replace_code(ni++, "\x75\x00", 2, label, l1);
					insert_code(ni++, "\x49", 1, 0, 0);
					insert_code(ni++, "\x74\x00", 2, 0, l2);
					insert_code(ni++, "\xe9\x00\x00\x00\x00", 5, 0, jmp_to_label);
					insert_code(ni++, "\x49", 1, l1, 0);
					insert_code(ni++, "\x90", 1, l2, 0);

					update_jumps();

					--ni;
					it = m_code.begin()+ni;
				}
			}

			offset += cinstr->size();
			++ni;
		}
	}
}

void obfusion::mangle_code()
{
	std::vector<codeinstr*> obf_code;

	DEBUG_PRINTF(__FUNCTION__": begin\n");

	update_cache();

	DEBUG_PRINTF(__FUNCTION__": cache updated\n");

	DEBUG_PRINTF(__FUNCTION__": jump chance - %u%%\n", m_jmp_perc);

	u32 dbgi = 0;
	u32 ki = 0;
	for ( std::vector<codeinstr*>::iterator it = m_code.begin(); it != m_code.end(); ++it )
	{
		if (dbgi % 1000 == 0)
			DEBUG_PRINTF(__FUNCTION__": %u/%u\n", dbgi, m_code.size());

		codeinstr *cinstr = (*it);
		if (cinstr)
		{
			if ( cinstr->is_data() == false )
			{
				if (obf_code.size() > 0 && (mt::rand_u32()%100) < m_jmp_perc)
				{
					// insert jump
					u32 to_label = cinstr->label();
					if (to_label == 0)
						to_label = get_next_label();

					insert_code(&obf_code, ki++, "\xeb\x00", 2, 0, to_label);

					ki = mt::rand_u32() % obf_code.size();
					while (ki<obf_code.size() && obf_code[ki]->is_data() == true) // make sure to not inject jumps where data resides
						ki = mt::rand_u32() % obf_code.size();

					u32 jmp_label = 0;
					if (ki < obf_code.size())
						jmp_label = obf_code[ki]->label();
					if (jmp_label == 0)
					{
						jmp_label = get_next_label();
						if (ki < obf_code.size())
						{
							obf_code[ki]->set_label(jmp_label);
							m_label_cache[jmp_label] = obf_code[ki];
						}
					}

					insert_code(&obf_code, ki++, "\xeb\x00", 2, 0, jmp_label);
					obf_code.insert(obf_code.begin()+ki, cinstr);
					cinstr->set_label(to_label);

					m_label_cache[to_label] = cinstr;
				}
				else
					obf_code.insert(obf_code.begin()+ki, cinstr);
			}
			else
				obf_code.insert(obf_code.begin()+ki, cinstr);

			++ki;
		}

		++dbgi;
	}

	DEBUG_PRINTF(__FUNCTION__": update_jumps()\n");

	m_code = obf_code;
	update_jumps();

	clear_cache();

	DEBUG_PRINTF(__FUNCTION__": finish\n");
}

void obfusion::update_jumps()
{
	for ( std::vector<codeinstr*>::iterator it = m_code.begin(); it != m_code.end(); ++it )
	{
		codeinstr *cinstr = (*it);
		if (cinstr)
		{
			if ( cinstr->is_data() == false && cinstr->jmp_to_label() > 0 )
			{
				if ( m_jmp_map.find(cinstr) == m_jmp_map.end() )
				{
					u32 jmp_to_label = cinstr->jmp_to_label();

					codeinstr *jinstr;
					if ( m_label_cache.find(jmp_to_label) != m_label_cache.end() )
						jinstr = m_label_cache[jmp_to_label];
					else
						jinstr = get_instr_by_label(jmp_to_label);

					if (jinstr)
						m_jmp_map[cinstr] = jinstr;
				}
			}
		}
	}
}

void obfusion::update_cache()
{
	clear_cache();
	for ( std::vector<codeinstr*>::iterator it = m_code.begin(); it != m_code.end(); ++it )
	{
		codeinstr *cinstr = (*it);
		if (cinstr)
		{
			if (cinstr->label() > 0)
				m_label_cache[cinstr->label()] = cinstr;
		}
	}
}

void obfusion::clear_cache()
{
	m_label_cache.clear();
}

bool obfusion::mod_jmp_instr(codeinstr* cinstr, s32 jmp_delta, modbuf* jmp_bytes)
{
	bool ret = true;
	jmp_bytes->clear();

	u8 op1 = cinstr->get_hde32()->opcode;
	u8 op2 = cinstr->get_hde32()->opcode2;
	u32 flags = cinstr->get_hde32()->flags;

	s32 dm = 0;

	if ( -128 <= jmp_delta && jmp_delta <= 127 )
	{
		// short jump

		if ( (flags & F_IMM8) == F_IMM8 )
		{
			// current jump is also short

			jmp_bytes->add<u8>(op1);
			jmp_bytes->add<s8>((s8)jmp_delta);
		}
		else if ( (flags & F_IMM32) == F_IMM32 )
		{
			// current jump is a far jump

			if (op1 == 0x0f) // jmp cond far
			{
				if (jmp_delta < 0)
					dm = 6-2; // instruction size difference
				jmp_bytes->add<u8>((op2 & 0x0f) | 0x70);
				jmp_bytes->add<s8>((s8)jmp_delta + dm);
			}
			else if (op1 == 0xe8) // call
			{
				jmp_bytes->add<u8>(op1);
				jmp_bytes->add<s32>(jmp_delta);
			}
			else if (op1 == 0xe9) // jmp
			{
				if (jmp_delta < 0)
					dm = 5-2; // instruction size difference
				jmp_bytes->add<u8>(0xeb);
				jmp_bytes->add<s8>((s8)jmp_delta + dm);
			}
			else
				ret = false;
		}
	}
	else
	{
		// far jump

		if ( (flags & F_IMM8) == F_IMM8 )
		{
			// current jump is a short jump

			if ((op1 & 0xf0) == 0x70) // jmp cond short
			{
				if (jmp_delta < 0)
					dm = 2-6;
				jmp_bytes->add<u8>(0x0f);
				jmp_bytes->add<u8>((op1 & 0x0f) | 0x80);
				jmp_bytes->add<s32>(jmp_delta + dm);
			}
			else if (op1 == 0xeb) // jmp short
			{
				if (jmp_delta < 0)
					dm = 2-5;
				jmp_bytes->add<u8>(0xe9);
				jmp_bytes->add<s32>(jmp_delta + dm);
			}
			else
				ret = false;
		}
		else if ( (flags & F_IMM32) == F_IMM32 )
		{
			// current jump is also a far jump

			if (op1 == 0x0f)
			{
				jmp_bytes->add<u8>(0x0f);
				jmp_bytes->add<u8>(op2);
				jmp_bytes->add<s32>(jmp_delta);
			}
			else
			{
				jmp_bytes->add<u8>(op1);
				jmp_bytes->add<s32>(jmp_delta);
			}
		}
	}

	return ret;
}

u32 obfusion::get_offset_by_instr(codeinstr* cinstr)
{
	u32 offset = 0;
	u32 ni = 0;

	for ( std::vector<codeinstr*>::iterator it = m_code.begin(); it != m_code.end(); ++it )
	{
		codeinstr *_cinstr = (*it);
		if (cinstr == _cinstr)
			return offset;

		offset += _cinstr->size();
		++ni;
	}
	return -1;
}

codeinstr* obfusion::get_instr_by_label(u32 label)
{
	u32 offset = 0;
	u32 ni = 0;

	for ( std::vector<codeinstr*>::iterator it = m_code.begin(); it != m_code.end(); ++it )
	{
		codeinstr *_cinstr = (*it);
		if (_cinstr)
		{
			if ( _cinstr->label() == label )
				return _cinstr;
		}

		offset += _cinstr->size();
		++ni;
	}
	return 0;
}

u32 obfusion::get_code_ni_by_offset(u32 _offset)
{
	u32 offset = 0;
	u32 ni = 0;

	for ( std::vector<codeinstr*>::iterator it = m_code.begin(); it != m_code.end(); ++it )
	{
		codeinstr *cinstr = (*it);
		if (cinstr)
		{
			if ( _offset == offset )
				return ni;

			offset += cinstr->size();
			++ni;
		}
	}
	return -1;
}

u32 obfusion::get_next_label()
{
	return m_cur_label++;
}