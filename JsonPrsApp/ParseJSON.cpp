#include "stdafx.h"
#include "ParseJSON.h"

namespace Morrin::JSON
{
inline  bool IsWhiteSpace( std::string_view::value_type value )
{
	//	�z���C�g�X�y�[�X�Ƃ��ċK�肳��Ă���Bisspace �Ƃ͂�����ƈႤ�̂Œ�`�����i��
	switch( value )
	{
	case ' ':
	case '\t':
	case '\r':
	case '\n':
		return true;
	}
	return false;
}
inline std::string_view::size_type SkipWhiteSpace( const std::string_view& rowData, std::string_view::size_type offset )
{
	while( offset < rowData.length() && IsWhiteSpace( rowData[offset] ) )
	{
		++offset;
	}
	return offset;
}
static bool ParseElement( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset );
static bool ParseElements( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset );

static bool ParseObject( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset );
static bool ParseArray( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset );
static bool ParseString( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset, NodeType type );
static bool ParseLiteralValue( ParseCallBack& proc, std::string_view& rowData, std::string_view::size_type& offset, const std::string_view& checkValue, NodeType type );

static bool ParseNumberValue( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset );

//	�G�������g�̃p�[�X
static bool ParseElement( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )
{
	_ASSERTE( offset < rowData.length() );
	offset = SkipWhiteSpace( rowData, offset );
	//	�z���C�g�X�y�[�X���X�L�b�v���ăf�[�^���c���Ă���ꍇ�̂ݏ�������(�c���Ă��Ȃ����Ƃ����蓾��)
	if( offset < rowData.length() )
	{
		switch( rowData[offset] )
		{
		case '{':	//	object
			if( !ParseObject( proc, rowData, offset ) )
			{
				return false;
			}
			break;
		case '[':	//	array
			if( !ParseArray( proc, rowData, offset ) )
			{
				return false;
			}
			break;
		case '"':	//	string
			//	�I�u�W�F�N�g�̓r���ł͂Ȃ��̂ŁA�I�u�W�F�N�g�L�[�ł͂Ȃ�
			if( !ParseString( proc, rowData, offset, NodeType::String ) )
			{
				return false;
			}
			break;
		case 't':	//	���e�����`�F�b�N == true
			if( !ParseLiteralValue( proc, rowData, offset, "true", NodeType::True ) )
			{
				return false;
			}
			break;
		case 'f':	//	���e�����`�F�b�N == false
			if( !ParseLiteralValue( proc, rowData, offset, "false", NodeType::False ) )
			{
				return false;
			}
			break;
		case 'n':	//	���e�����`�F�b�N == null
			if( !ParseLiteralValue( proc, rowData, offset, "null", NodeType::Null ) )
			{
				return false;
			}
			break;
		default:	//	���l�̂͂��B�ꉞ�A�T�[�V�������炢�͓���Ď��q�͂���
			if( !ParseNumberValue( proc, rowData, offset ) )
			{
				return false;
			}
			break;
		}
		//	�G�������g�Ɍ㑱����z���C�g�X�y�[�X�̓X�L�b�v����
		offset = SkipWhiteSpace( rowData, offset );
	}
	return true;
}
static bool ParseElements( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )
{
	while( offset < rowData.length() )
	{
		if( !ParseElement( proc, rowData, offset ) )
		{
			return false;
		}
		//	Elements �� ',' ���㑱����ꍇ�A������� Element������
		if( offset < rowData.length() && rowData[offset] == ',' )
		{
			++offset;	//	','�͌㑱�ŎQ�Ƃ���Ȃ�
		}
		else
		{
			break;
		}
	}
	//	�����𐳏�ɏI�������̂Ő����I������
	return true;
}
static bool ParseObject( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )
{
	_ASSERTE( rowData[offset] == '{' );	//	��O�ŃX�L�b�v���Ȃ��̂ŁA�����ōēx�l�`�F�b�N�ł���悤�ɂ��Ă���
	return false;
}
static bool ParseArray( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )
{
	_ASSERTE( rowData[offset] == '[' );	//	��O�ŃX�L�b�v���Ȃ��̂ŁA�����ōēx�l�`�F�b�N�ł���悤�ɂ��Ă���
	return false;
}
static bool ParseString( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset, NodeType type )
{
	_ASSERTE( rowData[offset] == '"' );	//	��O�ŃX�L�b�v���Ȃ��̂ŁA�����ōēx�l�`�F�b�N�ł���悤�ɂ��Ă���
	return false;
}
static bool ParseLiteralValue( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset, const std::string_view& checkValue, NodeType type )
{
	bool result = false;
	if( rowData.compare( offset, checkValue.length(), checkValue ) == 0 )
	{
		result = proc( type, rowData, offset, checkValue.length() );	//	�L���Ȃ̂́A�f�[�^�T�C�Y�����Ȃ̂ł��̕����Z�b�g����
		offset += checkValue.length();	//	Literal�f�[�^�T�C�Y�����X�L�b�v
	}
	return result;
}
static bool ParseNumberValue( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )
{
	//	���l�f�[�^�́A�����������́A- �Ŏn�܂�B
	_ASSERTE( isdigit( rowData[offset] ) || rowData[offset] == '-' );	//	���l�܂��́A- �Ŏn�܂�
	return false;
}
///////////////////////////////////////////////////////////////////////////////
//	global Functions.
bool APIENTRY ParseJSON( const std::string_view& rowData, ParseCallBack& proc )
{
	std::string_view::size_type offset = 0;
	//	�J�n��ʒm
	if( !proc( NodeType::StartParse, rowData, offset, std::string_view::npos ) )
	{
		return false;
	}
	while( offset < rowData.length() )
	{
		offset = SkipWhiteSpace( rowData, offset );
		//	�z���C�g�X�y�[�X�����O������Ȃ��Ȃ����炻�̎��_�ŏI���(Element �̑O���WhiteSpace������)
		if( offset >= rowData.length() )
		{
			break;
		}
		//	���炩�̗��R�Ńp�[�X�����s����(�ʏ�́A�R�[���o�b�N����~��Ԃ���)��A�����͏I��
		if( !ParseElement( proc, rowData, offset ) )
		{
			break;
		}
	}
	//	�I����ʒm�Boffset �̈ʒu�Ŕ���\�Ȃ̂ŕK�v������΂�������邱�ƂƂ���
	return proc( NodeType::EndParse, rowData, offset, std::string_view::npos );
}
}
