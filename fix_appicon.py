from pathlib import Path
p = Path('src/app/AppRegistry.cpp')
lines = p.read_text().splitlines(keepends=True)
start = None
for i, ln in enumerate(lines):
    if 'appIcon(const AppDefinition& app' in ln and i > 45:
        start = i
        break
end = start
depth = 0
for i in range(start, len(lines)):
    depth += lines[i].count('{') - lines[i].count('}')
    if depth == 0 and i > start:
        end = i + 1
        break
print(f'appIcon spans lines {start+1} to {end}')
new_appIcon = '''static QString
appIcon(const AppDefinition& app, int role = 0)
{
    if (!qobject_cast<QGuiApplication*>(QCoreApplication::instance()))
        return {};
    const auto packaged = ":/config/app-icons/" + app.launchConfiguration.value("icon", app.id).toString() + ".svg";
    if (app.type != "web" && app.type != "desktop" && app.type != "android" && !QFile::exists(packaged))
        return {};
    static const QRegularExpression desktopName("^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$");
    const auto name = resolveAppIconName(app, desktopName);
    QIcon icon;
    if (QFile::exists(packaged))
    {
        if (!resolvePackagedIcon(app, role, icon))
            return {};
    }
    else if (QDir::isAbsolutePath(name))
    {
        if (!resolveAbsolutePathIcon(name, icon))
            return {};
    }
    else
    {
        if (!desktopName.match(name).hasMatch())
            return {};
        if (!resolveThemeIcon(name, QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation), icon))
            return {};
    }
    QByteArray bytes;
    if (!finalizeIconImage(icon, packaged, role, bytes))
        return {};
    auto profile = QCoreApplication::instance()->property("isoldeProfile").toString();
    if (profile.isEmpty())
        profile = "default";
    const auto path = cacheAppIcon(bytes, profile, desktopName);
    if (path.isEmpty())
        return {};
    return QUrl::fromLocalFile(path).toString();
}

'''
new_lines = lines[:start] + [new_appIcon] + lines[end:]
p.write_text(''.join(new_lines))
print(f'Replaced appIcon. Total lines: {len(new_lines)}')