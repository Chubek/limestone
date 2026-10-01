let s:default_commands = [
      \ '@brief', '@details', '@param', '@tparam', '@return', '@retval',
      \ '@throws', '@see', '@since', '@deprecated', '@note', '@warning',
      \ '@bug', '@todo', '@code', '@endcode', '@ingroup', '@file', '@author'
      \ ]

function! s:ezdox_home() abort
  return exists('$EZDOX_HOME') && !empty($EZDOX_HOME) ? $EZDOX_HOME : expand('~/.ezdox')
endfunction

function! ezdox#commands() abort
  let path = s:ezdox_home() . '/manifests/doxygen-commands.yaml'
  if !filereadable(path)
    return copy(s:default_commands)
  endif
  let commands = []
  for line in readfile(path)
    let match = matchlist(line, 'title:\s*["'']\?\\\([A-Za-z][A-Za-z0-9_]*\)')
    if !empty(match)
      call add(commands, '@' . match[1])
    endif
  endfor
  return empty(commands) ? copy(s:default_commands) : sort(uniq(commands))
endfunction

function! s:insert_lines(lines) abort
  call append(line('.') - 1, a:lines)
endfunction

function! ezdox#insert_block(...) abort
  let kind = a:0 ? a:1 : 'function'
  if kind ==# 'file'
    call s:insert_lines([
          \ '/**',
          \ ' * @file ' . expand('%:t'),
          \ ' * @brief TODO: describe this file.',
          \ ' */'
          \ ])
  elseif kind ==# 'class'
    call s:insert_lines([
          \ '/**',
          \ ' * @brief TODO: describe this type.',
          \ ' * @details TODO: add class responsibilities and invariants.',
          \ ' */'
          \ ])
  elseif kind ==# 'field'
    call s:insert_lines(['///< @brief TODO: describe this field.'])
  else
    call s:insert_lines([
          \ '/**',
          \ ' * @brief TODO: describe this function.',
          \ ' * @param name TODO: describe parameter.',
          \ ' * @return TODO: describe return value.',
          \ ' */'
          \ ])
  endif
endfunction

function! ezdox#insert_command(...) abort
  let command = a:0 ? a:1 : '@brief'
  let prefix = getline('.') =~# '^\s*\*' ? ' * ' : ''
  execute 'normal! a' . prefix . command . ' '
  startinsert!
endfunction

function! ezdox#inventory() abort
  new
  setlocal buftype=nofile bufhidden=wipe noswapfile
  file EZDox-Doxygen-Commands
  call setline(1, ezdox#commands())
  setlocal nomodifiable
endfunction

function! ezdox#validate_config(...) abort
  let config = a:0 ? a:1 : ''
  if empty(config)
    for candidate in ['EZDox.yaml', 'EZDox.yam', 'EZDox.json', 'EZDox.sexp', 'EZDox.xml']
      if filereadable(candidate)
        let config = candidate
        break
      endif
    endfor
  endif
  if empty(config)
    echoerr 'No EZDox config found'
    return
  endif
  execute '!ezdox-cli config validate -C ' . shellescape(config)
endfunction

function! ezdox#complete_block_kind(arglead, cmdline, cursorpos) abort
  return filter(['function', 'class', 'file', 'field'], 'v:val =~ "^" . a:arglead')
endfunction

function! ezdox#complete_command(arglead, cmdline, cursorpos) abort
  return filter(ezdox#commands(), 'v:val =~ "^" . a:arglead')
endfunction
