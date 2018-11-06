//	���x�D��^JSON�p�[�T�[ C++17��

//	�R���Z�v�g
//	�ڎw���ő��I������R�s�[���Ȃ��I
//	�ł��邾���^�͊���̂��̂��g���I
//	JSON�f�[�^�̏����̋K��� http://www.json.org/json-ja.html ���Q��(2018/11/05���_)

//	VC++�ȊO�ł̓r���h���ĂȂ��I

#pragma once

#include <string_view>
#include <functional>
#include <utility>

namespace Morrin::JSON
{
enum NotificationId
{
	StartParse,		//	�p�[�X�J�n(���ꂩ��f�[�^�������Ă���)
	EndParse,		//	�p�[�X�I��(����ȏ�f�[�^�������Ă��邱�Ƃ͂Ȃ�)
	//	�����^�̊J�n�ƏI��(������}�[�J�[)
	StartObject,	//	'{'
	EndObject,		//	'}'
	StartArray,		//	'['
	EndArray,		//	']'
	//	�z��v�f�̃Z�p���[�^
	Comma,
	//	�P��^�̒ʒm
	Key,			//	�I�u�W�F�N�g�̃L�[(���ۂ͕�����)
	String,			//	������f�[�^
	Digit,			//	���l�f�[�^
	True,			//	true
	False,			//	false
	Null,			//	null
};

//	�p�[�X���ʂ̃R�[���o�b�N�󂯎�胁�\�b�h( bool func( Morrin::JSON::NodeType nodeType, const std::string_view& node ) )
using ParseCallBack = std::function<bool( NotificationId id, const std::string_view& value )> const;

//	JSON�p�[�X�G���W���{��
bool APIENTRY ParseJSON( const std::string_view& rowData, ParseCallBack& proc );
std::string APIENTRY ExtractEscapeString( const std::string_view& escapeValue );

//	JSON�f�[�^�̔ėp�^�N���X�����H�Ƃ��āB�B�Bpair<key,value> value�� std::any �ɂ��邩�Avariant<int,double,u8string,vector<int,double,u8string>>�ɂ��邩�B�B�B���Ȃ��H

}//	Morin::JSON
