/// Color, in RGBA format. Corresponds to the C++ type `ColorRGBA`.
///
/// The color is represented by red, green, blue and alpha values between `0.0`
/// and `1.0`.
///
/// See also <https://en.wikipedia.org/wiki/RGBA_color_model>.
///
/// # Examples
///
/// ```
/// use ddnet_base::ColorRGBA;
///
/// let white = ColorRGBA { r: 1.0, g: 1.0, b: 1.0, a: 1.0 };
/// let black = ColorRGBA { r: 0.0, g: 0.0, b: 0.0, a: 1.0 };
/// let red = ColorRGBA { r: 1.0, g: 0.0, b: 0.0, a: 1.0 };
/// let transparent = ColorRGBA { r: 0.0, g: 0.0, b: 0.0, a: 0.0 };
///
/// // #ffa500
/// let ddnet_logo_color = ColorRGBA { r: 1.0, g: 0.6470588235294118, b: 0.0, a: 1.0 };
/// ```
#[derive(Clone, Copy, Debug, PartialEq)]
#[repr(C)]
pub struct ColorRGBA {
    /// Red
    pub r: f32,
    /// Green
    pub g: f32,
    /// Blue
    pub b: f32,
    /// Alpha (i.e. opacity. `0.0` means fully transparent, `1.0`
    /// nontransparent).
    pub a: f32,
}

unsafe impl cxx::ExternType for ColorRGBA {
    type Id = cxx::type_id!("ColorRGBA");
    type Kind = cxx::kind::Trivial;
}

/// Color, in HSLA format. Corresponds to the C++ type `ColorHSLA`.
///
/// The color is represented by hue, saturation, lightness and alpha values
/// between `0.0` and `1.0`.
///
/// See also <https://en.wikipedia.org/wiki/HSL_and_HSV>.
///
/// # Examples
///
/// ```
/// use ddnet_base::ColorHSLA;
///
/// let white = ColorHSLA { h: 0.0, s: 0.0, l: 1.0, a: 1.0 };
/// let black = ColorHSLA { h: 0.0, s: 0.0, l: 0.0, a: 1.0 };
/// let red = ColorHSLA { h: 0.0, s: 1.0, l: 0.5, a: 1.0 };
/// let transparent = ColorHSLA { h: 0.0, s: 0.0, l: 0.0, a: 0.0 };
///
/// // #ffa500
/// let ddnet_logo_color = ColorHSLA { h: 0.10784314, s: 1.0, l: 0.5, a: 1.0 };
/// ```
#[derive(Clone, Copy, Debug, PartialEq)]
#[repr(C)]
pub struct ColorHSLA {
    /// Hue
    pub h: f32,
    /// Saturation
    pub s: f32,
    /// Lightness
    pub l: f32,
    /// Alpha (i.e. opacity. `0.0` means fully transparent, `1.0`
    /// nontransparent).
    pub a: f32,
}

unsafe impl cxx::ExternType for ColorHSLA {
    type Id = cxx::type_id!("ColorHSLA");
    type Kind = cxx::kind::Trivial;
}
