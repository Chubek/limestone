\NeedsTeXFormat{LaTeX2e}
\ProvidesClass{ezdox}[2026/06/24 EZDox documentation class]
\LoadClass[11pt,a4paper]{article}
\RequirePackage[utf8]{inputenc}
\RequirePackage[T1]{fontenc}
\RequirePackage{lmodern}
\RequirePackage{hyperref}
\RequirePackage{listings}
\RequirePackage{xcolor}
\RequirePackage{booktabs}
\RequirePackage{geometry}
\geometry{margin=2.5cm}

\definecolor{codebg}{RGB}{248,248,248}
\definecolor{codeframe}{RGB}{200,200,200}
\definecolor{keyword}{RGB}{0,119,170}
\definecolor{string}{RGB}{0,136,0}

\lstset{
  backgroundcolor=\color{codebg},
  frame=single,
  rulecolor=\color{codeframe},
  basicstyle=\ttfamily\small,
  keywordstyle=\color{keyword}\bfseries,
  stringstyle=\color{string},
  breaklines=true,
  showstringspaces=false
}
