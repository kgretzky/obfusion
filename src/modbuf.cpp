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
#include "modbuf.h"

modbuf::modbuf(void)
{
	m_buf = 0;
	m_chunk_size = CHUNK_SIZE;
	clear();
}

modbuf::modbuf(int chunk_size)
{
	m_buf = 0;
	m_chunk_size = chunk_size;
	clear();
}

modbuf::~modbuf(void)
{
	clear();
}

void modbuf::clear()
{
	if ( m_buf != 0 )
		delete[] m_buf;
	m_buf = 0;
	m_alloc_size = 0;
	m_size = 0;
}

void modbuf::add_data( void *data, int len )
{
	if ( m_size + len >= m_alloc_size )
	{
		while ( m_size + len >= m_alloc_size )
			m_alloc_size += m_chunk_size;

		unsigned char *new_buf = new unsigned char[m_alloc_size];
		memset( new_buf, 0, m_alloc_size );

		if ( m_buf != 0 )
		{
			memcpy( new_buf, m_buf, m_size );
			delete[] m_buf;
		}
		m_buf = new_buf;
	}

	memcpy( m_buf + m_size, data, len );
	m_size += len;
}

void modbuf::add_size( int len )
{
	unsigned char *zeros = new unsigned char[len];
	memset( zeros, 0, len );

	add_data( zeros, len );
	delete[] zeros;
}
