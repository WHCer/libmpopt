#!/usr/bin/env python3

import sys

from mpopt import ct
from mpopt import utils


if __name__ == '__main__':
    input_filename, = sys.argv[1:]
    with utils.smart_open(input_filename, 'rt') as f:
        model, bimap = ct.convert_jug_to_ct(ct.parse_jug_model(f))
    tracker = ct.construct_tracker(model)
    tracker.run()

    rounding = ct.ExactNeighbourRounding(model, tracker)
    rounding.run()
    primals = rounding.best_primals

    lb = tracker.lower_bound()
    ub = primals.evaluate()

    print('final lb={} ub={} gap={}%'.format(lb,
                                             ub,
                                             100.0 * (ub - lb) / abs(lb)))

    with open('tracking.sol', 'w') as f:
        ct.format_jug_primals(primals, bimap, f)
