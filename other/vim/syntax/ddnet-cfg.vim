" Vim syntax file
" Language: ddnet config files (https://github.com/ddnet/ddnet)

if exists("b:current_syntax")
  finish
endif

syntax match settingName "^\w*"

syntax match comment "#.*"
syntax match value "\s\w*"
syntax match escapeQuote  "\\\""
syntax match number  "\s[0-9]\+"
syntax match ip  "\s\d\+\.\d\+\.\d\+\.\d\+\(:\d\+\)\="

syntax region string start='"' end='"' contains=escapeQuote

hi def link settingName Identifier

hi def link comment Comment
hi def link value Constant
hi def link ip Constant
hi def link string String
hi def link number Number
hi def link escapeQuote SpecialChar

let b:current_syntax = "ddnet-cfg"
