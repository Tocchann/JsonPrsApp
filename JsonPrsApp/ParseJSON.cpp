#include "stdafx.h"
#include "ParseJSON.h"

#include <string>

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
static bool ParseMember( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset );
static bool ParseValues( std::function<bool( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )>&& parseProc,
						 ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset );

static bool ParseObject( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset );
static bool ParseArray( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset );
static bool ParseStringValue( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset );
static bool ParseLiteralValue( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset, const std::string_view& checkValue, NotificationId type );
static bool ParseNumberValue( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset );

static std::string_view ExtractStringValue( const std::string_view& rowData, std::string_view::size_type& offset );

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
			if( !ParseStringValue( proc, rowData, offset ) )
			{
				return false;
			}
			break;
		case 't':	//	���e�����`�F�b�N == true
			if( !ParseLiteralValue( proc, rowData, offset, "true", NotificationId::True ) )
			{
				return false;
			}
			break;
		case 'f':	//	���e�����`�F�b�N == false
			if( !ParseLiteralValue( proc, rowData, offset, "false", NotificationId::False ) )
			{
				return false;
			}
			break;
		case 'n':	//	���e�����`�F�b�N == null
			if( !ParseLiteralValue( proc, rowData, offset, "null", NotificationId::Null ) )
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
static bool ParseMember( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )
{
	offset = SkipWhiteSpace( rowData, offset );	//	'{' �̎�����󔒕��������O
	_ASSERTE( rowData[offset] == '"' );
	auto key = ExtractStringValue( rowData, offset );
	if( !proc( NotificationId::Key, key ) )
	{
		return false;
	}
	offset = SkipWhiteSpace( rowData, offset );
	_ASSERTE( rowData[offset] == ':' );
	//	�f�[�^�����������ꍇ�͂����ŏI���ł���
	if( rowData[offset++] != ':' )
	{
		return false;
	}
	if( !ParseElement( proc, rowData, offset ) )
	{
		return false;
	}
	offset = SkipWhiteSpace( rowData, offset );
	return true;
}
static bool ParseValues( std::function<bool( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )>&& parseProc,
						 ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )
{
	while( offset < rowData.length() )
	{
		if( !parseProc( proc, rowData, offset ) )
		{
			return false;
		}
		//	Elements/Members �� ',' ���㑱����ꍇ�A������� Element/Member ������
		if( offset < rowData.length() && rowData[offset] == ',' )
		{
			//	��؂��ʒm����
			if( !proc( NotificationId::Comma, rowData.substr( offset++, 1 ) ) )
			{
				return false;	//	�L�����Z�����ꂽ
			}
		}
		else
		{
			break;
		}
	}
	//	�����𐳏�ɏI�������̂Ő����I������	//	Elements �̏I���͉�������Ȃ�
	return true;
}
static bool ParseObject( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )
{
	_ASSERTE( rowData[offset] == '{' );	//	��O�ŃX�L�b�v���Ȃ��̂ŁA�����ōēx�l�`�F�b�N�ł���悤�ɂ��Ă���
	if( !proc( NotificationId::StartObject, rowData.substr( offset++, 1 ) ) )
	{
		return false;
	}
	if( !ParseValues( ParseMember, proc, rowData, offset ) )
	{
		return false;
	}
	_ASSERTE( rowData[offset] == '}' );	//	��O�ŃX�L�b�v���Ȃ��̂ŁA�����ōēx�l�`�F�b�N�ł���悤�ɂ��Ă���
	if( rowData[offset] != '}' )
	{
		return false;
	}
	return proc( NotificationId::EndObject, rowData.substr( offset++, 1 ) );
}
static bool ParseArray( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )
{
	_ASSERTE( rowData[offset] == '[' );	//	��O�ŃX�L�b�v���Ȃ��̂ŁA�����ōēx�l�`�F�b�N�ł���悤�ɂ��Ă���
	if( !proc( NotificationId::StartArray, rowData.substr( offset++, 1 ) ) )
	{
		return false;
	}
	if( !ParseValues( ParseElement, proc, rowData, offset ) )
	{
		return false;
	}
	offset = SkipWhiteSpace( rowData, offset );
	_ASSERTE( rowData[offset] == ']' );	//	��O�ŃX�L�b�v���Ȃ��̂ŁA�����ōēx�l�`�F�b�N�ł���悤�ɂ��Ă���
	if( rowData[offset] != ']' )
	{
		return false;
	}
	return proc( NotificationId::EndArray, rowData.substr( offset++, 1 ) );
}
static bool ParseStringValue( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )
{
	_ASSERTE( rowData[offset] == '"' );	//	��O�ŃX�L�b�v���Ȃ��̂ŁA�����ōēx�l�`�F�b�N�ł���悤�ɂ��Ă���
	auto value = ExtractStringValue( rowData, offset );	//	������ '"' �̎��܂ňړ��B���ۂ̒l�͎Q�Ƃ��Ȃ����ǁA�����Ő؂�o���Ă���̂Œ����͌����
	return proc( NotificationId::String, value );
}
static bool ParseLiteralValue( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset, const std::string_view& checkValue, NotificationId type )
{
	if( rowData.compare( offset, checkValue.length(), checkValue ) == 0 )
	{
		offset += checkValue.length();
		return proc( type, checkValue );
	}
	return false;
}
static bool ParseNumberValue( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )
{
	//	���l�f�[�^�́A�����������́A- �Ŏn�܂�B
	_ASSERTE( isdigit( rowData[offset] ) || rowData[offset] == '-' );	//	���l�܂��́A- �Ŏn�܂�
	//	�����΂���Ƃ͌���Ȃ��̂ŁA���l�f�[�^�͕�����̂܂ܓn���B���z�́Aint/double ������Ȃ񂾂낤���ǁB�B�B
	auto start = rowData.data()+offset;
	char* end = nullptr;
	auto doubleValue = strtod( start, &end );
	auto length = end-start;
	auto value = rowData.substr( offset, length );
	offset += length;
	return proc( NotificationId::Digit, value );
}
static std::string_view ExtractStringValue( const std::string_view& rowData, std::string_view::size_type& offset )
{
	_ASSERTE( rowData[offset] == '"' );	//	��O�ŃX�L�b�v���Ȃ��̂ŁA�����ōēx�l�`�F�b�N�ł���悤�ɂ��Ă���
	++offset;	//	'"' ���X�L�b�v
	auto start = offset;
	//	�e�L�X�g�̏I�[�����܂ŃX�e�b�v����
	while( offset < rowData.length() && rowData[offset] != '"' )
	{
		//	�G�X�P�[�v�L��
		if( rowData[offset++] == '\\' )
		{
			switch( rowData[offset++] )
			{
			case 'u':
				//	�K���4���������ǔO�̂��߁B���i�`�F�b�N���Ȃ��ꍇ��+4�ŃX�L�b�v���Ă悢
				for( int cnt = 0 ; cnt < 4 ; ++cnt, ++offset )
				{
					//	�{���͓r���ŏI���Ƃ����ꓹ�f���x����ˁB�B�B
					if( !isxdigit( rowData[offset] ) )
					{
						return false;
					}
				}
				break;
			//	���i�`�F�b�N���Ȃ��ꍇ�́A���̂�����̓R�����g�A�E�g�ł悢
			case '"':
			case '\\':
			case '/':
			case 'b':
			case 'n':
			case 'r':
			case 't':
				break;
			default:
				return false;
			}
		}
	}
	_ASSERTE( rowData[offset] == '"' );	//	��O�ŃX�L�b�v���Ȃ��̂ŁA�����ōēx�l�`�F�b�N�ł���悤�ɂ��Ă���
	auto result = rowData.substr( start, offset-start );
	++offset;
	return result;
}
///////////////////////////////////////////////////////////////////////////////
//	global Functions.
bool APIENTRY ParseJSON( const std::string_view& rowData, ParseCallBack& proc )
{
	std::string_view::size_type offset = 0;
	//	�J�n��ʒm(�󕶎���)
	if( !proc( NotificationId::StartParse, rowData ) )
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
	return proc( NotificationId::EndParse, rowData.substr( offset ) );	//	�Ȃɂ��c���Ă�Ƃ����z��őS�����荞��ł��(����Ƀl�X�g�I�ȏ����������Ă��悢�Ƃ�������)
}
//	�e�L�X�g�̃G�X�P�[�v��������������
std::string APIENTRY ExtractEscapeString( const std::string_view& escapeValue )
{
	std::string_view::size_type pos = 0;
	std::string resultU8Str;
	auto prevPos = pos;
	while( pos < escapeValue.length() )
	{
		//	�G�X�P�[�v�L��
		if( escapeValue[pos] == '\\' )
		{
			if( prevPos != std::string_view::npos )
			{
				resultU8Str += escapeValue.substr( prevPos, pos-prevPos );
			}
			prevPos = std::string_view::npos;	//	�G�X�P�[�v���������Ȃ̂Ń��Z�b�g
			++pos;	//	����
			switch( escapeValue[pos] )
			{
			case 'u':
				//	�K���4���������ǔO�̂��߁B���i�`�F�b�N���Ȃ��ꍇ��+4�ŃX�L�b�v���Ă悢
				{
					wchar_t ch = 0;
					for( int cnt = 0 ; cnt < 4 && isxdigit( escapeValue[pos] ) ; ++cnt, ++pos )
					{
						if( isdigit( escapeValue[pos] ) )
						{
							ch = (ch << 4) + (escapeValue[pos] - '0');
						}
						else
						{
							ch = (ch<<4) + ( (toupper( escapeValue[pos] ) - 'A') + 0xA);
						}
					}
					char utf8str[4+1];	//	UTF8�͍ő�4�����ɕ��������
					WideCharToMultiByte( CP_UTF8, 0, &ch, 1, utf8str, 4+1, nullptr, nullptr );
					resultU8Str += utf8str;
				}
				break;
			//	���i�`�F�b�N���Ȃ��ꍇ�́A���̂�����̓R�����g�A�E�g�ł悢
			case '"':
			case '\\':
			case '/':
			case 'b':
			case 'n':
			case 'r':
			case 't':
				resultU8Str += escapeValue[pos];
				++pos;
				break;
			default:
				return false;
			}
		}
		else
		{
			if( prevPos == std::string_view::npos )
			{
				prevPos = pos;
			}
			++pos;
		}
	}
	if( prevPos != std::string_view::npos )
	{
		resultU8Str += escapeValue.substr( prevPos, pos-prevPos );
	}
	return resultU8Str;
}
}//	namespace Morrin::JSON
