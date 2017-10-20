Glew = {
	OptFind = function (name, required)
		local check = function(option, settings)
			option.value = false
			option.use_winlib = 0
			option.use_macosxframwork = 0

			if platform == "win32" then
				option.value = true
				option.use_winlib = 32
			elseif platform == "win64" then
				option.value = true
				option.use_winlib = 64
			elseif platform == "macosx" and string.find(settings.config_name, "32") then
				option.value = true
				option.use_macosxframwork = 32
			elseif platform == "macosx" and string.find(settings.config_name, "64") then
				option.value = true
				option.use_macosxframwork = 64
			elseif platform == "linux" and arch == "ia32" then
				option.value = true
			elseif platform == "linux" and arch == "amd64" then
				option.value = true
			end
		end

		local apply = function(option, settings)
			settings.cc.includes:Add("ddnet-libs/glew/include")

			if option.use_winlib > 0 then
				if option.use_winlib == 32 then
					settings.link.libpath:Add("ddnet-libs/glew/windows/lib32")
				elseif option.use_winlib == 64 then
					settings.link.libpath:Add("ddnet-libs/glew/windows/lib64")
				end
				
				settings.link.libs:Add("glew32")
			elseif option.use_macosxframwork > 0 then
				-- no glew
			else
				settings.link.libs:Add("GLEW")
			end
		end

		local save = function(option, output)
			output:option(option, "value")
			output:option(option, "use_winlib")
			output:option(option, "use_macosxframwork")
		end

		local display = function(option)
			if option.value == true then
				if option.use_winlib == 32 then return "using supplied win32 libraries" end
				if option.use_winlib == 64 then return "using supplied win64 libraries" end
				return "using value"
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
