'use strict';

console.log('Hello world');

{
    const FormattableDatetime = require('./FormattableDatetime').FormattableDatetime;
    console.log(new Date().toLocaleString());
    console.log(new FormattableDatetime().formatLocaleStandard());
    console.log(new FormattableDatetime().formatLocaleForPath());
    console.log(new FormattableDatetime(1998, 0, 25, 1, 2, 3).formatLocale('yyyyyy(yyyy(yy))/M/d H:m:s'));
}