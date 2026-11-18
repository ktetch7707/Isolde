from pathlib import Path
p = Path('src/media/GuideRules.cpp')
s = p.read_text()
old = '''bool
validateRuleLeafValue(const QVariant& value, bool numeric, const QString& op)
{
    if (op == "one-of" || op == "in-range")
        return validateRuleList(value, numeric, op);
    const auto scalar = validateScalar(value, numeric);
    if (!scalar)
        return false;
    if (op == "is" || op == "is-not")
        return true;
    if (numeric)
        return op == "before" || op == "after";
    return op == "contains" || op == "does-not-contain" || op == "starts-with";
}'''
new = '''bool
validateRuleLeafValue(const QVariant& value, bool numeric, const QString& op)
{
    if (op == "one-of" || op == "in-range")
        return validateRuleList(value, numeric, op);
    const auto scalar = validateScalar(value, numeric);
    if (!scalar)
        return false;
    return validateRuleLeafComparison(value, numeric, op);
}

bool
validateRuleLeafComparison(const QVariant& value, bool numeric, const QString& op)
{
    if (op == "is" || op == "is-not")
        return true;
    if (numeric)
        return op == "before" || op == "after";
    return op == "contains" || op == "does-not-contain" || op == "starts-with";
}'''
if old in s:
    s = s.replace(old, new, 1)
    p.write_text(s)
    print("Replaced validateRuleLeafValue")
else:
    print("Could not find old text")
