/**
 * �������`�p�̃N���X
 */
class FormattableDatetime extends Date {
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
     * �ʏ�t�H�[�}�b�g('yyyy/MM/dd HH:mm:ss')�ɐ��`����
     * @returns ���`�ς݂̕�����
     */
    formatLocaleStandard() {
        return this.formatLocale('yyyy/MM/dd HH:mm:ss')
    }

    /**
     * �p�X�ɖ��ߍ��߂�t�H�[�}�b�g('yyyy-MM-dd_HH-mm-ss')�ɐ��`����
     * @returns ���`�ς݂̕�����
     */
    formatLocaleForPath() {
        return this.formatLocale('yyyy-MM-dd_HH-mm-ss');
    }
}

module.exports = {
    FormattableDatetime
}