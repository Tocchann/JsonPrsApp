#include "stdafx.h"
#include "ParseJSON.h"

namespace Morrin::JSON
{
inline  bool IsWhiteSpace( std::string_view::value_type value )
{
	//	ホワイトスペースとして規定されている。isspace とはちょっと違うので定義を厳格化
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
			//	オブジェクトの途中ではないので、オブジェクトキーではない
			if( !ParseString( proc, rowData, offset, NodeType::String ) )
			{
				return false;
			}
			break;
		case 't':	//	リテラルチェック == true
			if( !ParseLiteralValue( proc, rowData, offset, "true", NodeType::True ) )
			{
				return false;
			}
			break;
		case 'f':	//	リテラルチェック == false
			if( !ParseLiteralValue( proc, rowData, offset, "false", NodeType::False ) )
			{
				return false;
			}
			break;
		case 'n':	//	リテラルチェック == null
			if( !ParseLiteralValue( proc, rowData, offset, "null", NodeType::Null ) )
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
static bool ParseElements( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )
{
	while( offset < rowData.length() )
	{
		if( !ParseElement( proc, rowData, offset ) )
		{
			return false;
		}
		//	Elements は ',' が後続する場合、もう一回 Elementが来る
		if( offset < rowData.length() && rowData[offset] == ',' )
		{
			++offset;	//	','は後続で参照されない
		}
		else
		{
			break;
		}
	}
	//	処理を正常に終了したので成功終了する
	return true;
}
static bool ParseObject( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )
{
	_ASSERTE( rowData[offset] == '{' );	//	手前でスキップしないので、ここで再度値チェックできるようにしておく
	return false;
}
static bool ParseArray( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )
{
	_ASSERTE( rowData[offset] == '[' );	//	手前でスキップしないので、ここで再度値チェックできるようにしておく
	return false;
}
static bool ParseString( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset, NodeType type )
{
	_ASSERTE( rowData[offset] == '"' );	//	手前でスキップしないので、ここで再度値チェックできるようにしておく
	return false;
}
static bool ParseLiteralValue( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset, const std::string_view& checkValue, NodeType type )
{
	bool result = false;
	if( rowData.compare( offset, checkValue.length(), checkValue ) == 0 )
	{
		result = proc( type, rowData, offset, checkValue.length() );	//	有効なのは、データサイズだけなのでその分をセットする
		offset += checkValue.length();	//	Literalデータサイズ分をスキップ
	}
	return result;
}
static bool ParseNumberValue( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )
{
	//	数値データは、数字もしくは、- で始まる。
	_ASSERTE( isdigit( rowData[offset] ) || rowData[offset] == '-' );	//	数値または、- で始まる
	return false;
}
///////////////////////////////////////////////////////////////////////////////
//	global Functions.
bool APIENTRY ParseJSON( const std::string_view& rowData, ParseCallBack& proc )
{
	std::string_view::size_type offset = 0;
	//	開始を通知
	if( !proc( NodeType::StartParse, rowData, offset, std::string_view::npos ) )
	{
		return false;
	}
	while( offset < rowData.length() )
	{
		offset = SkipWhiteSpace( rowData, offset );
		//	ホワイトスペースを除外したらなくなったらその時点で終わる(Element の前後にWhiteSpaceがある)
		if( offset >= rowData.length() )
		{
			break;
		}
		//	何らかの理由でパースが失敗した(通常は、コールバックが停止を返した)ら、処理は終了
		if( !ParseElement( proc, rowData, offset ) )
		{
			break;
		}
	}
	//	終了を通知。offset の位置で判定可能なので必要があればそれを見ることとする
	return proc( NodeType::EndParse, rowData, offset, std::string_view::npos );
}
}
