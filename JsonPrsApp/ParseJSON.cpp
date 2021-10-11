#include "stdafx.h"
#include "ParseJSON.h"

namespace Wankuma::JSON
{
inline  bool IsWhiteSpace( std::string_view::value_type value )
{
	//	�z���C�g�X�y�[�X�Ƃ��ċK�肳��Ă���Bisspace �Ƃ͂�����ƈႤ�̂Œ�`�����i��
	switch( value )
	{
	case u8' ':
	case u8'\t':
	case u8'\r':
	case u8'\n':
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
		case u8'{':	//	object
			if( !ParseObject( proc, rowData, offset ) )
			{
				return false;
			}
			break;
		case u8'[':	//	array
			if( !ParseArray( proc, rowData, offset ) )
			{
				return false;
			}
			break;
		case u8'"':	//	string
			//	�I�u�W�F�N�g�̓r���ł͂Ȃ��̂ŁA�I�u�W�F�N�g�L�[�ł͂Ȃ�
			if( !ParseStringValue( proc, rowData, offset ) )
			{
				return false;
			}
			break;
		case u8't':	//	���e�����`�F�b�N == true
			if( !ParseLiteralValue( proc, rowData, offset, u8"true", NotificationId::True ) )
			{
				return false;
			}
			break;
		case u8'f':	//	���e�����`�F�b�N == false
			if( !ParseLiteralValue( proc, rowData, offset, u8"false", NotificationId::False ) )
			{
				return false;
			}
			break;
		case u8'n':	//	���e�����`�F�b�N == null
			if( !ParseLiteralValue( proc, rowData, offset, u8"null", NotificationId::Null ) )
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
	//	��u���b�N�̏ꍇ������
	if( rowData[offset] == u8'}' )
	{
		return true;
	}
	_ASSERTE( rowData[offset] == u8'"' );
	auto key = ExtractStringValue( rowData, offset );
	if( !proc( NotificationId::Key, key ) )
	{
		return false;
	}
	offset = SkipWhiteSpace( rowData, offset );
	_ASSERTE( rowData[offset] == ':' );
	//	�f�[�^�����������ꍇ�͂����ŏI���ł���
	if( rowData[offset++] != u8':' )
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
	_ASSERTE( rowData[offset] == u8'{' );	//	��O�ŃX�L�b�v���Ȃ��̂ŁA�����ōēx�l�`�F�b�N�ł���悤�ɂ��Ă���
	if( !proc( NotificationId::StartObject, rowData.substr( offset++, 1 ) ) )
	{
		return false;
	}
	if( !ParseValues( ParseMember, proc, rowData, offset ) )
	{
		return false;
	}
	_ASSERTE( rowData[offset] == u8'}' );	//	��O�ŃX�L�b�v���Ȃ��̂ŁA�����ōēx�l�`�F�b�N�ł���悤�ɂ��Ă���
	if( rowData[offset] != u8'}' )
	{
		return false;
	}
	return proc( NotificationId::EndObject, rowData.substr( offset++, 1 ) );
}
static bool ParseArray( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )
{
	_ASSERTE( rowData[offset] == u8'[' );	//	��O�ŃX�L�b�v���Ȃ��̂ŁA�����ōēx�l�`�F�b�N�ł���悤�ɂ��Ă���
	if( !proc( NotificationId::StartArray, rowData.substr( offset++, 1 ) ) )
	{
		return false;
	}
	_ASSERTE( offset < rowData.length() );
	offset = SkipWhiteSpace( rowData, offset );
	if( offset < rowData.length() && rowData[offset] != u8']' )
	{
		if( !ParseValues( ParseElement, proc, rowData, offset ) )
		{
			return false;
		}
		offset = SkipWhiteSpace( rowData, offset );
	}
	_ASSERTE( rowData[offset] == u8']' );	//	��O�ŃX�L�b�v���Ȃ��̂ŁA�����ōēx�l�`�F�b�N�ł���悤�ɂ��Ă���
	if( rowData[offset] != u8']' )
	{
		return false;
	}
	return proc( NotificationId::EndArray, rowData.substr( offset++, 1 ) );
}
static bool ParseStringValue( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )
{
	_ASSERTE( rowData[offset] == u8'"' );	//	��O�ŃX�L�b�v���Ȃ��̂ŁA�����ōēx�l�`�F�b�N�ł���悤�ɂ��Ă���
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
	//	�l�̐��������l�����Ȃ��Ȃ�A�f���~�^(,]} or WhiteSpace))�܂ŃX�L�b�v����ق����������ǁA�l�̐������͊m�ۂ���B
	auto start = rowData.data()+offset;
	char* end = nullptr;
	auto doubleValue = strtod( start, &end );
	auto length = end-start;
	auto value = rowData.substr( offset, length );
	offset += length;
	//	�㑱�́AWhiteSpace �̂ق��A���̃f�[�^�̂��߂̋�؂�(,�I�u�W�F�N�g�̊J�n�ƏI���A�z��̊J�n�ƏI��)�̂����ꂩ�ɂȂ�
	_ASSERTE( IsWhiteSpace( rowData[offset] ) || rowData[offset] == ',' || rowData[offset] == '{' || rowData[offset] == '}' || rowData[offset] == '[' || rowData[offset] == ']' );
	return proc( NotificationId::Digit, value );
}
static std::string_view ExtractStringValue( const std::string_view& rowData, std::string_view::size_type& offset )
{
	_ASSERTE( rowData[offset] == u8'"' );	//	��O�ŃX�L�b�v���Ȃ��̂ŁA�����ōēx�l�`�F�b�N�ł���悤�ɂ��Ă���
	++offset;	//	'"' ���X�L�b�v
	auto start = offset;
	//	�e�L�X�g�̏I�[�����܂ŃX�e�b�v����
	while( offset < rowData.length() && rowData[offset] != '"' )
	{
		//	�G�X�P�[�v�L��
		if( rowData[offset++] == u8'\\' )
		{
			switch( rowData[offset++] )
			{
			case 'u':
				//	�K���4�����B������HEX�����邱�Ƃ��`�F�b�N
				if( !isxdigit( rowData[offset++] ) ){	return false;	}
				if( !isxdigit( rowData[offset++] ) ){	return false;	}
				if( !isxdigit( rowData[offset++] ) ){	return false;	}
				if( !isxdigit( rowData[offset++] ) ){	return false;	}
				break;
			//	���i�`�F�b�N���Ȃ��ꍇ�́A���̂�����̓R�����g�A�E�g�ł悢
			case u8'"': case u8'/': case u8'b': case u8'\\':
			case u8'n': case u8'r': case u8't':
				break;
			default:
				//	�m��Ȃ��L�����G�X�P�[�v����Ă�����G���[�ɂ���(�m��Ȃ�����)
				return false;
			}
		}
	}
	_ASSERTE( rowData[offset] == u8'"' );	//	��O�ŃX�L�b�v���Ȃ��̂ŁA�����ōēx�l�`�F�b�N�ł���悤�ɂ��Ă���
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
	if( !proc( NotificationId::StartParse, u8"" ) )
	{
		return false;
	}
	while( offset < rowData.length() )
	{
		//	���炩�̗��R�Ńp�[�X�����s����(�ʏ�́A�R�[���o�b�N����~��Ԃ���)��A�����͏I��
		if( !ParseElement( proc, rowData, offset ) )
		{
			break;
		}
	}
	//	�I����ʒm�Boffset �̈ʒu�Ŕ���\�Ȃ̂ŕK�v������΂�������邱�ƂƂ���
	_ASSERTE( offset <= rowData.length() );
	//	�I�[�o�[���Ă���Ɨ�O���o��̂ŁA���[�𒴂��Ȃ��悤�ɂ���(�v���O�����I�ɂ͖����𒴂��邱�Ƃ͂Ȃ��͂��Ȃ̂����B�B�B)
	if( offset > rowData.length() )	offset = rowData.length();
	return proc( NotificationId::EndParse, rowData.substr( offset ) );	//	�Ȃɂ��c���Ă�Ƃ����z��őS�����荞��ł��(����Ƀl�X�g�I�ȏ����������Ă��悢�Ƃ�������)
}
//	�e�L�X�g�̃G�X�P�[�v��������������
std::string APIENTRY UnescapeString( const std::string_view& value )
{
	std::string_view::size_type pos = 0;
	std::string resultStr;
	auto prevPos = pos;
	auto SetPrevStr = []( decltype(value) value, decltype(pos) pos, decltype(pos) prevPos, decltype(resultStr)& resultStr )
	{
		if( prevPos != value.npos )
		{
			resultStr += value.substr( prevPos, pos-prevPos );
		}
	};
	std::wstring u16str;
	while( pos < value.length() )
	{
		//	�G�X�P�[�v�L��
		if( value[pos] == u8'\\' )
		{
			SetPrevStr( value, pos, prevPos, resultStr );
			prevPos = std::string_view::npos;	//	�G�X�P�[�v�������Z�b�g�����̂ŁA�ȑO�̈ʒu���N���A�[
			++pos;
			if( value[pos] == u8'u' )
			{
				++pos;
				wchar_t ch = 0;
				for( int cnt = 0 ; cnt < 4 && isxdigit( value[pos] ) ; ++cnt, ++pos )
				{
					if( isdigit( value[pos] ) )
					{
						ch = (ch << 4) + (value[pos] - u8'0');
					}
					else
					{
						ch = (ch<<4) + ((toupper( value[pos] ) - u8'A') + 0xA);
					}
				}
				u16str += ch;
			}
			else
			{
				if( !u16str.empty() )
				{
					resultStr += CW2A( u16str.c_str(), CP_UTF8 );
					u16str.clear();
				}
				switch( value[pos] )
				{
				//	���̂܂ܗ��p���Ă�������
				case u8'"': case u8'\\': case u8'/':	resultStr += value[pos];	break;
				//	���䕶���Ȃ̂ł����ƕϊ�����
				case u8'b':						resultStr += u8'\b';	break;
				case u8'n':						resultStr += u8'\n';	break;
				case u8'r':						resultStr += u8'\r';	break;
				case u8't':						resultStr += u8'\t';	break;
				default:	return false;
				}
				++pos;
			}
		}
		else
		{
			if( !u16str.empty() )
			{
				resultStr += CW2A( u16str.c_str(), CP_UTF8 );
				u16str.clear();
			}
			if( prevPos == std::string_view::npos )
			{
				prevPos = pos;
			}
			++pos;
		}
	}
	SetPrevStr( value, pos, prevPos, resultStr );
	return resultStr;
}
std::wstring APIENTRY UnescapeWstring( const std::string_view& value )
{
	std::string_view::size_type pos = 0;
	std::wstring resultStr;
	auto prevPos = pos;
	auto SetPrevStr = []( decltype(value) value, decltype(pos) pos, decltype(pos) prevPos, decltype(resultStr)& resultStr )
	{
		if( prevPos != std::string_view::npos )
		{
			int length = static_cast<int>(pos-prevPos);
			const auto& u8str = value.substr( prevPos, length );
			auto u16len = MultiByteToWideChar( CP_UTF8, 0, u8str.data(), length, nullptr, 0 );
			std::wstring u16str;
			u16str.resize( u16len );
			MultiByteToWideChar( CP_UTF8, 0, u8str.data(), length, u16str.data(), static_cast<int>( u16str.size() ) );
			resultStr += u16str;
		}
	};
	while( pos < value.length() )
	{
		//	�G�X�P�[�v�L��
		if( value[pos] == u8'\\' )
		{
			SetPrevStr( value, pos, prevPos, resultStr );
			prevPos = std::string_view::npos;	//	�G�X�P�[�v�������Z�b�g�����̂ŁA�ȑO�̈ʒu���N���A�[
			++pos;	//	����
			if( value[pos] == u8'u' )
			{
				//	�����R�[�h�G�X�P�[�v���ƃT���Q�[�g�y�A�Ȃǂ��l������̂ŁA��ʂ蕶�������Ă��珈������悤�ɕύX�B
				++pos;	//	����
				wchar_t ch = 0;
				for( int cnt = 0 ; cnt < 4 && isxdigit( value[pos] ) ; ++cnt, ++pos )
				{
					if( isdigit( value[pos] ) )
					{
						ch = (ch << 4) + (value[pos] - u8'0');
					}
					else
					{
						ch = (ch<<4) + ((toupper( value[pos] ) - u8'A') + 0xA);
					}
				}
				//	UNICODE�����Ȃ̂ł��̂܂ܒǉ�����΂���
				resultStr += ch;
			}
			else
			{
				switch( value[pos] )
				{
				//	���̂܂ܗ��p���Ă�������
				case u8'"': case u8'\\': case u8'/':	resultStr += value[pos];	break;
				//	���䕶���ɕϊ�����
				case u8'b':						resultStr += L'\b';	break;
				case u8'n':						resultStr += L'\n';	break;
				case u8'r':						resultStr += L'\r';	break;
				case u8't':						resultStr += L'\t';	break;
				default:	return false;
				}
				++pos;
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
	SetPrevStr( value, pos, prevPos, resultStr );
	return resultStr;
}
std::wstring APIENTRY EscapeString( const std::wstring_view& value, bool escapeNonAscii )
{
	std::wstring_view::size_type pos = 0;
	std::wstring resultStr;
	auto prevPos = pos;
	auto SetPrevStr = []( decltype(value) value, decltype(pos) pos, decltype(pos) prevPos, decltype(resultStr)& resultStr )
	{
		if( prevPos != std::wstring_view::npos )
		{
			resultStr += value.substr( prevPos, pos-prevPos );
			prevPos = std::wstring_view::npos;	//	�G�X�P�[�v�������Z�b�g�����̂ŁA�ȑO�̈ʒu���N���A�[
		}
		return prevPos;
	};
	while( pos < value.length() )
	{
		//	1�o�C�g�ŕ\���ł��Ȃ��������G�X�P�[�v����
		if( value[pos] > 0xFF && escapeNonAscii )
		{
			//	16�i���Ńe�L�X�g�����郁�\�b�h���Ȃ��̂ŁAultow ���g���B
			wchar_t	buff[_MAX_ULTOSTR_BASE16_COUNT];	//	�O�̂��ߍő�l���m�ۂ��Ă���
			_ultow_s( value[pos], buff, 16 );	// sizeof(wchar_t) == sizeof(WORD) �����ǁB�B�B�Ȃ�
			prevPos = SetPrevStr( value, pos, prevPos, resultStr );
			resultStr += L"\\u";
			resultStr += buff;
		}
		else
		{
			LPCWSTR addStr = nullptr;
			switch( value[pos] )
			{
			case L'\b':	addStr = L"\\b";	break;
			case L'\n':	addStr = L"\\n";	break;
			case L'\r':	addStr = L"\\r";	break;
			case L'\t':	addStr = L"\\t";	break;
			case L'/':	addStr = L"\\/";	break;
			case L'"':	addStr = L"\\\"";	break;
			case L'\\':	addStr = L"\\\\";	break;
			default:
				if( prevPos == std::wstring_view::npos )
				{
					prevPos = pos;
				}
			}
			if( addStr != nullptr )
			{
				prevPos = SetPrevStr( value, pos, prevPos, resultStr );
				resultStr += addStr;
			}
		}
		++pos;
	}
	SetPrevStr( value, pos, prevPos, resultStr );
	return resultStr;
}

}//	namespace Wankuma::JSON
