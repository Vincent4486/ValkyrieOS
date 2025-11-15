;; -*- lexical-binding: t; -*-

(TeX-add-style-hook
 "TheValkyrieOperatingSystem"
 (lambda ()
   (TeX-add-to-alist 'LaTeX-provided-class-options
                     '(("book" "14pt" "openany") ("article" "12pt" "14pt" "openany")))
   (TeX-add-to-alist 'LaTeX-provided-package-options
                     '(("graphicx" "") ("inputenc" "utf8") ("fancyhdr" "") ("titling" "")))
   (TeX-run-style-hooks
    "latex2e"
    "article"
    "art10"
    "graphicx"
    "inputenc"
    "fancyhdr"
    "titling"))
 :latex)

