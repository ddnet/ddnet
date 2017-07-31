Mysql = {
	OptFind = function (name, required)
		local check = function(option, settings)
			option.value = false
			option.lib_path = nil

			if IsNegativeTerm(ScriptArgs[name .. ".use_mysqlconfig"]) then
				option.use_mysqlconfig = false
			elseif IsPositiveTerm(ScriptArgs[name .. ".use_mysqlconfig"]) then
				option.use_mysqlconfig = true
			end

			if platform == "linux" then
				-- Bundled libs are totally broken on lots of systems, new builds are not better, so let's just force the system ones
				if option.use_mysqlconfig == nil then
					option.use_mysqlconfig = true
				end
			end

			if ExecuteSilent("mysql_config --version") == 0 then
				option.value = true
				if option.use_mysqlconfig == nil then
					option.use_mysqlconfig = true
				end
			end

			if option.use_mysqlconfig == nil then
				option.use_mysqlconfig = false
			end

			if platform == "win32" then
				option.value = true
			elseif platform == "win64" then
				option.value = true
			elseif platform == "macosx" and string.find(settings.config_name, "32") then
				option.value = true
			elseif platform == "macosx" and string.find(settings.config_name, "64") then
				option.value = true
			elseif platform == "linux" and arch == "ia32" then
				option.value = true
			elseif platform == "linux" and arch == "amd64" then
				option.value = true
			end
		end

		local apply = function(option, settings)
			if option.use_mysqlconfig == true then
				settings.cc.flags:Add("`mysql_config --cflags`")
				settings.link.flags:Add("`mysql_config --libs`")
				settings.link.libs:Add("mysqlcppconn")
			else
				settings.cc.includes:Add("ddnet-libs/mysql/include")

				if family ~= "windows" then
					settings.link.libs:Add("mysqlcppconn-static")
					settings.link.libs:Add("mysqlclient")
					settings.link.libs:Add("dl")
					if platform ~= "macosx" then
						settings.link.libs:Add("rt")
					end
				end

				if platform == "macosx" and string.find(settings.config_name, "32") then
					settings.link.libpath:Add("ddnet-libs/mysql/mac/lib32")
				elseif platform == "macosx" and string.find(settings.config_name, "64") then
					settings.link.libpath:Add("ddnet-libs/mysql/mac/lib64")
				elseif platform == "linux" then
					settings.link.libpath:Add("ddnet-libs/mysql/linux/lib64")
					settings.link.libpath:Add("ddnet-libs/mysql/linux/lib32")
				elseif platform == "windows" then
					settings.link.libpath:Add("ddnet-libs/mysql/windows")
				end
			end
		end

		local save = function(option, output)
			output:option(option, "value")
			output:option(option, "use_mysqlconfig")
		end

		local display = function(option)
			if option.value == true then
				if option.use_mysqlconfig == true then return "using mysql_config" end
				return "using bundled libs"
			else
				if option.required then
					return "not found (required)"
				else
					return "not found (optional)"
				end
			end
		end

		local o = MakeOption(name, 0, check, save, display)
		o.Apply = apply
		o.include_path = nil
		o.lib_path = nil
		o.required = required
		return o
	end
}
