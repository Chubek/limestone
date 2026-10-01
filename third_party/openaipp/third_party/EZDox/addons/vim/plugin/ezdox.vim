if exists('g:loaded_ezdox_plugin')
  finish
endif
let g:loaded_ezdox_plugin = 1

command! -nargs=? -complete=customlist,ezdox#complete_block_kind EZDoxDocBlock call ezdox#insert_block(<f-args>)
command! -nargs=? -complete=customlist,ezdox#complete_command EZDoxCommand call ezdox#insert_command(<f-args>)
command! EZDoxCommands call ezdox#inventory()
command! -nargs=? -complete=file EZDoxValidate call ezdox#validate_config(<f-args>)

nnoremap <silent> <Plug>(ezdox-doc-function) :EZDoxDocBlock function<CR>
nnoremap <silent> <Plug>(ezdox-doc-class) :EZDoxDocBlock class<CR>
nnoremap <silent> <Plug>(ezdox-doc-file) :EZDoxDocBlock file<CR>
nnoremap <silent> <Plug>(ezdox-commands) :EZDoxCommands<CR>
