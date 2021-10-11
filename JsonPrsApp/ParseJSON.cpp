#include "stdafx.h"
#include "ParseJSON.h"

namespace Wankuma::JSON
{
inline  bool IsWhiteSpace( std::string_view::value_type value )
{
	//	ホワイトスペースとして規定されている。isspace とはちょっと違うので定義を厳格化
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

//	エレメントのパース
static bool ParseElement( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )
{
	_ASSERTE( offset < rowData.length() );
	offset = SkipWhiteSpace( rowData, offset );
	//	ホワイトスペースをスキップしてデータが残っている場合のみ処理する(残っていないこともあり得る)
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
			//	オブジェクトの途中ではないので、オブジェクトキーではない
			if( !ParseStringValue( proc, rowData, offset ) )
			{
				return false;
			}
			break;
		case u8't':	//	リテラルチェック == true
			if( !ParseLiteralValue( proc, rowData, offset, u8"true", NotificationId::True ) )
			{
				return false;
			}
			break;
		case u8'f':	//	リテラルチェック == false
			if( !ParseLiteralValue( proc, rowData, offset, u8"false", NotificationId::False ) )
			{
				return false;
			}
			break;
		case u8'n':	//	リテラルチェック == null
			if( !ParseLiteralValue( proc, rowData, offset, u8"null", NotificationId::Null ) )
			{
				return false;
			}
			break;
		default:	//	数値のはず。一応アサーションくらいは入れて自衛はする
			if( !ParseNumberValue( proc, rowData, offset ) )
			{
				return false;
			}
			break;
		}
		//	エレメントに後続するホワイトスペースはスキップする
		offset = SkipWhiteSpace( rowData, offset );
	}
	return true;
}
static bool ParseMember( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )
{
	offset = SkipWhiteSpace( rowData, offset );	//	'{' の次から空白部分を除外
	//	空ブロックの場合がある
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
	//	データがおかしい場合はそこで終了でいい
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
		//	Elements/Members は ',' が後続する場合、もう一回 Element/Member が来る
		if( offset < rowData.length() && rowData[offset] == ',' )
		{
			//	区切りを通知する
			if( !proc( NotificationId::Comma, rowData.substr( offset++, 1 ) ) )
			{
				return false;	//	キャンセルされた
			}
		}
		else
		{
			break;
		}
	}
	//	処理を正常に終了したので成功終了する	//	Elements の終わりは何も言わない
	return true;
}
static bool ParseObject( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )
{
	_ASSERTE( rowData[offset] == u8'{' );	//	手前でスキップしないので、ここで再度値チェックできるようにしておく
	if( !proc( NotificationId::StartObject, rowData.substr( offset++, 1 ) ) )
	{
		return false;
	}
	if( !ParseValues( ParseMember, proc, rowData, offset ) )
	{
		return false;
	}
	_ASSERTE( rowData[offset] == u8'}' );	//	手前でスキップしないので、ここで再度値チェックできるようにしておく
	if( rowData[offset] != u8'}' )
	{
		return false;
	}
	return proc( NotificationId::EndObject, rowData.substr( offset++, 1 ) );
}
static bool ParseArray( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )
{
	_ASSERTE( rowData[offset] == u8'[' );	//	手前でスキップしないので、ここで再度値チェックできるようにしておく
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
	_ASSERTE( rowData[offset] == u8']' );	//	手前でスキップしないので、ここで再度値チェックできるようにしておく
	if( rowData[offset] != u8']' )
	{
		return false;
	}
	return proc( NotificationId::EndArray, rowData.substr( offset++, 1 ) );
}
static bool ParseStringValue( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )
{
	_ASSERTE( rowData[offset] == u8'"' );	//	手前でスキップしないので、ここで再度値チェックできるようにしておく
	auto value = ExtractStringValue( rowData, offset );	//	末尾の '"' の次まで移動。実際の値は参照しないけど、ここで切り出しているので長さは見れる
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
	//	数値データは、数字もしくは、- で始まる。
	_ASSERTE( isdigit( rowData[offset] ) || rowData[offset] == '-' );	//	数値または、- で始まる
	//	値の正当性を考慮しないなら、デリミタ(,]} or WhiteSpace))までスキップするほうが速いけど、値の正当性は確保する。
	auto start = rowData.data()+offset;
	char* end = nullptr;
	auto doubleValue = strtod( start, &end );
	auto length = end-start;
	auto value = rowData.substr( offset, length );
	offset += length;
	//	後続は、WhiteSpace のほか、次のデータのための区切り(,オブジェクトの開始と終了、配列の開始と終了)のいずれかになる
	_ASSERTE( IsWhiteSpace( rowData[offset] ) || rowData[offset] == ',' || rowData[offset] == '{' || rowData[offset] == '}' || rowData[offset] == '[' || rowData[offset] == ']' );
	return proc( NotificationId::Digit, value );
}
static std::string_view ExtractStringValue( const std::string_view& rowData, std::string_view::size_type& offset )
{
	_ASSERTE( rowData[offset] == u8'"' );	//	手前でスキップしないので、ここで再度値チェックできるようにしておく
	++offset;	//	'"' をスキップ
	auto start = offset;
	//	テキストの終端部分までステップする
	while( offset < rowData.length() && rowData[offset] != '"' )
	{
		//	エスケープ記号
		if( rowData[offset++] == u8'\\' )
		{
			switch( rowData[offset++] )
			{
			case 'u':
				//	規定で4文字。ちゃんとHEXがあることをチェック
				if( !isxdigit( rowData[offset++] ) ){	return false;	}
				if( !isxdigit( rowData[offset++] ) ){	return false;	}
				if( !isxdigit( rowData[offset++] ) ){	return false;	}
				if( !isxdigit( rowData[offset++] ) ){	return false;	}
				break;
			//	厳格チェックしない場合は、このあたりはコメントアウトでよい
			case u8'"': case u8'/': case u8'b': case u8'\\':
			case u8'n': case u8'r': case u8't':
				break;
			default:
				//	知らない記号がエスケープされていたらエラーにする(知らないもの)
				return false;
			}
		}
	}
	_ASSERTE( rowData[offset] == u8'"' );	//	手前でスキップしないので、ここで再度値チェックできるようにしておく
	auto result = rowData.substr( start, offset-start );
	++offset;
	return result;
}
///////////////////////////////////////////////////////////////////////////////
//	global Functions.
bool APIENTRY ParseJSON( const std::string_view& rowData, ParseCallBack& proc )
{
	std::string_view::size_type offset = 0;
	//	開始を通知(空文字列)
	if( !proc( NotificationId::StartParse, u8"" ) )
	{
		return false;
	}
	while( offset < rowData.length() )
	{
		//	何らかの理由でパースが失敗した(通常は、コールバックが停止を返した)ら、処理は終了
		if( !ParseElement( proc, rowData, offset ) )
		{
			break;
		}
	}
	//	終了を通知。offset の位置で判定可能なので必要があればそれを見ることとする
	_ASSERTE( offset <= rowData.length() );
	//	オーバーしていると例外が出るので、末端を超えないようにする(プログラム的には末尾を超えることはないはずなのだが。。。)
	if( offset > rowData.length() )	offset = rowData.length();
	return proc( NotificationId::EndParse, rowData.substr( offset ) );	//	なにか残ってるという想定で全部送り込んでやる(さらにネスト的な処理があってもよいという判定)
}
//	テキストのエスケープ処理を解決する
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
		//	エスケープ記号
		if( value[pos] == u8'\\' )
		{
			SetPrevStr( value, pos, prevPos, resultStr );
			prevPos = std::string_view::npos;	//	エスケープ文字をセットしたので、以前の位置をクリアー
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
				//	そのまま利用していい文字
				case u8'"': case u8'\\': case u8'/':	resultStr += value[pos];	break;
				//	制御文字なのでちゃんと変換する
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
		//	エスケープ記号
		if( value[pos] == u8'\\' )
		{
			SetPrevStr( value, pos, prevPos, resultStr );
			prevPos = std::string_view::npos;	//	エスケープ文字をセットしたので、以前の位置をクリアー
			++pos;	//	無視
			if( value[pos] == u8'u' )
			{
				//	文字コードエスケープだとサロゲートペアなども考えられるので、一通り文字化してから処理するように変更。
				++pos;	//	無視
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
				//	UNICODE文字なのでそのまま追加すればいい
				resultStr += ch;
			}
			else
			{
				switch( value[pos] )
				{
				//	そのまま利用していい文字
				case u8'"': case u8'\\': case u8'/':	resultStr += value[pos];	break;
				//	制御文字に変換する
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
			prevPos = std::wstring_view::npos;	//	エスケープ文字をセットしたので、以前の位置をクリアー
		}
		return prevPos;
	};
	while( pos < value.length() )
	{
		//	1バイトで表現できない文字をエスケープする
		if( value[pos] > 0xFF && escapeNonAscii )
		{
			//	16進数でテキスト化するメソッドがないので、ultow を使う。
			wchar_t	buff[_MAX_ULTOSTR_BASE16_COUNT];	//	念のため最大値を確保しておく
			_ultow_s( value[pos], buff, 16 );	// sizeof(wchar_t) == sizeof(WORD) だけど。。。なｗ
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
