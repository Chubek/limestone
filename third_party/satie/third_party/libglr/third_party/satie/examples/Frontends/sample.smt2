; Satie frontend sample: 2 <= x <= 5 over QF_LIA
(set-logic QF_LIA)
(declare-const x Int)
(assert (>= x 2))
(assert (<= x 5))
(check-sat)
