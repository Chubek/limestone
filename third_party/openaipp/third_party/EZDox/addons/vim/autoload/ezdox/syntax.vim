function! ezdox#syntax#enable() abort
  if exists('b:ezdox_docstring_syntax')
    return
  endif
  let b:ezdox_docstring_syntax = 1

  syntax keyword ezdoxDoxygenCommand contained
        \ brief details param tparam return retval throws exception see since
        \ deprecated note warning bug todo code endcode ingroup defgroup file
        \ author version date pre post invariant attention par example

  syntax match ezdoxDoxygenCommand contained /[@\\][A-Za-z][A-Za-z0-9_]*/
  syntax match ezdoxTodo contained /\<TODO\>/

  syntax region ezdoxLineDocComment start=+^\s*///\|///<+ end=+$+
        \ contains=ezdoxDoxygenCommand,ezdoxTodo,@Spell keepend
  syntax region ezdoxBlockDocComment start=+/\*\*+ end=+\*/+
        \ contains=ezdoxDoxygenCommand,ezdoxTodo,@Spell keepend
  syntax region ezdoxBangDocComment start=+/\*!+ end=+\*/+
        \ contains=ezdoxDoxygenCommand,ezdoxTodo,@Spell keepend

  highlight default link ezdoxLineDocComment Comment
  highlight default link ezdoxBlockDocComment Comment
  highlight default link ezdoxBangDocComment Comment
  highlight default link ezdoxDoxygenCommand Keyword
  highlight default link ezdoxTodo Todo
endfunction

