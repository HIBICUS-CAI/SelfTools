/**
 * �����g���N���X
 */
class DatetimeExt extends Date {
    /**
     * �R���X�g���N�^
     * @param  {...any} args Date�̃R���X�g���N�^�ɓn�������
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
     * �w�肵���p�^�[���Ő��`����
     * @param {string} pattern �t�H�[�}�b�g�p�^�[���A'yyyy/MM/dd HH:mm:ss'�݂����ȁBHour��24���Ԑ�����i'HH'=='hh'�j�BYear�ȊO�����J�b�g�s�B
     * @returns ���`�ς݂̕�����
     */
    formatLocale(pattern) {
        const padOrCut = (origin, demandLength, doseCut = false) => {
            let result = origin.toString();
            const resultLength = result.length;
            if (doseCut && resultLength > demandLength) {
                result = result.slice(resultLength - demandLength);
            } else if (resultLength < demandLength) {
                const padding = '0';
                result = result.padStart(demandLength, padding);
            }

            return result;
        }

        return pattern
            .replace(/(y+)/g, hit => padOrCut(super.getFullYear(), hit.length, true))
            .replace(/(M+)/g, hit => padOrCut(super.getMonth() + 1, hit.length))
            .replace(/(d+)/g, hit => padOrCut(super.getDate(), hit.length))
            .replace(/([Hh]+)/g, hit => padOrCut(super.getHours(), hit.length))
            .replace(/(m+)/g, hit => padOrCut(super.getMinutes(), hit.length))
            .replace(/(s+)/g, hit => padOrCut(super.getSeconds(), hit.length));
    }

    /**
     * �ʏ�t�H�[�}�b�g('yyyy/MM/dd HH:mm:ss')�ɐ��`����
     * @returns ���`�ς݂̕�����
     */
    formatLocaleStandard() {
        return this.formatLocale('yyyy/MM/dd HH:mm:ss');
    }

    /**
     * �p�X�ɖ��ߍ��߂�t�H�[�}�b�g('yyyy-MM-dd_HH-mm-ss')�ɐ��`����
     * @returns ���`�ς݂̕�����
     */
    formatLocaleForPath() {
        return this.formatLocale('yyyy-MM-dd_HH-mm-ss');
    }

    /**
     * �w�肵���N���𑝂₷
     * @param {number} years �����N��
     */
    addYears(years) {
        super.setFullYear(super.getFullYear() + years);
    }

    /**
     * �w�肵�������𑝂₷
     * @param {number} months ��������
     */
    addMonths(months) {
        super.setMonth(super.getMonth() + months);
    }

    /**
     * �w�肵�������𑝂₷
     * @param {number} days ��������
     */
    addDays(days) {
        super.setDate(super.getDate() + days);
    }

    /**
     * �w�肵�����ԕ��𑝂₷
     * @param {number} hours ��������
     */
    addHours(hours) {
        super.setHours(super.getHours() + hours);
    }

    /**
     * �w�肵�������𑝂₷
     * @param {number} minutes ������
     */
    addMinutes(minutes) {
        super.setMinutes(super.getMinutes() + minutes);
    }

    /**
     * �w�肵���b���𑝂₷
     * @param {number} seconds �����b
     */
    addSeconds(seconds) {
        super.setSeconds(super.getSeconds() + seconds);
    }

    /**
     * �w�肵���~���b���𑝂₷
     * @param {number} milliSeconds �����~���b
     */
    addMilliSeconds(milliSeconds) {
        super.setMilliseconds(super.getMilliseconds() + milliSeconds);
    }
}

module.exports = {
    DatetimeExt
};