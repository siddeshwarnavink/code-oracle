(ql:quickload :cffi)

(defpackage :webscrape
  (:use :cl :cffi))

(in-package :webscrape)

(load-foreign-library "./.build/libcopypasta.so")

(defcfun ("gfg_scrape" gfg-scrape) :int
  (url :string path :string))

(let ((status (gfg-scrape
               "https://www.geeksforgeeks.org/create-a-form-using-reactjs/"
               "output.js")))
  (format t "gfg_scrape returned: ~A~%" status))

(sb-ext:exit :code 0)
