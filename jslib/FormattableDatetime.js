/**
 * 日時整形用のクラス
 */
class FormattableDatetime extends Date {
    /**
     * コンストラクタ
     * @param  {...any} args Dateのコンストラクタに渡せるもの
     */
    constructor(...args) {
        switch (args.length) {
            case 0: super(); break;
            case 1: super(args[0]); break;
            case 2: super(args[0], args[1]); break;
            case 3: super(args[0], args[1], args[2]); break;
            case 4: super(args[0], args[1], args[2], args[3]); break;
            case 5: super(args[0], args[1], args[2], args[3], args[4]); break;
            case 6: super(args[0], args[1], args[2], args[3], args[4], args[5]); break;
            case 7: super(args[0], args[1], args[2], args[3], args[4], args[5], args[6]); break;
            default: throw new Error(`invalid args: [${args.join()}]`);
        }
    }

    /**
     * 指定したパターンで整形する
     * @param {string} pattern フォーマットパターン、'yyyy/MM/dd HH:mm:ss'みたいな。Hourは24時間制限定（'HH'=='hh'）。Year以外桁数カット不可。
     * @returns 整形済みの文字列
     */
    formatLocale(pattern) {
        let result = pattern;

        function padOrCut(origin, demandLength, doseCut = false) {
            let result = origin.toString();
            let resultLength = result.length;
            if (doseCut && resultLength > demandLength) {
                result = result.slice(resultLength - demandLength);
            } else if (resultLength < demandLength) {
                const padding = '0';
                result = result.padStart(demandLength, padding);
            }

            return result;
        }

        result = result
            .replace(/(y+)/g, hit => padOrCut(super.getFullYear(), hit.length, true))
            .replace(/(M+)/g, hit => padOrCut(super.getMonth() + 1, hit.length))
            .replace(/(d+)/g, hit => padOrCut(super.getDate(), hit.length))
            .replace(/([Hh]+)/g, hit => padOrCut(super.getHours(), hit.length))
            .replace(/(m+)/g, hit => padOrCut(super.getMinutes(), hit.length))
            .replace(/(s+)/g, hit => padOrCut(super.getSeconds(), hit.length));
        return result;
    }

    /**
     * 通常フォーマット('yyyy/MM/dd HH:mm:ss')に整形する
     * @returns 整形済みの文字列
     */
    formatLocaleStandard() {
        return this.formatLocale('yyyy/MM/dd HH:mm:ss')
    }

    /**
     * パスに埋め込めるフォーマット('yyyy-MM-dd_HH-mm-ss')に整形する
     * @returns 整形済みの文字列
     */
    formatLocaleForPath() {
        return this.formatLocale('yyyy-MM-dd_HH-mm-ss');
    }
}

module.exports = {
    FormattableDatetime
}