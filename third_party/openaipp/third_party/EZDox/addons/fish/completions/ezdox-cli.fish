complete -c ezdox-cli -f
complete -c ezdox-cli -n '__fish_use_subcommand' -a 'help version paths config bundle find generate install run doctor'
complete -c ezdox-cli -n '__fish_seen_subcommand_from config' -a 'scaffold validate print run'
complete -c ezdox-cli -n '__fish_seen_subcommand_from bundle' -a 'build install list remove inspect'
complete -c ezdox-cli -s C -l config -r -d 'EZDox config file'
complete -c ezdox-cli -s O -l output -r -d 'Documentation output directory'
complete -c ezdox-cli -s t -l target -a 'HTML LaTeX Manpage ROFF XML' -d 'Output target'
complete -c ezdox-cli -l template -r -d 'Template root'
complete -c ezdox-cli -l clean -d 'Clean output before generating'
complete -c ezdox-cli -l profile -d 'Print timing/profile details'

