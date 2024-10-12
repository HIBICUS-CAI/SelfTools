'use strict';

console.log('Hello world');

{
    const DatetimeExt = require('./DatetimeExt').DatetimeExt;
    console.log(new Date().toLocaleString());
    console.log(new DatetimeExt().formatLocaleStandard());
    console.log(new DatetimeExt().formatLocaleForPath());
    console.log(new DatetimeExt(1998, 0, 25, 1, 2, 3).formatLocale('yyyyyy(yyyy(yy))/M/d H:m:s'));

    let date0 = new DatetimeExt();
    date0.addDays(20);
    console.log(date0.formatLocaleStandard());
}