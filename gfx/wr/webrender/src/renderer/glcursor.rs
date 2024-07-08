/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ImageFormat, ImageBufferKind};
use api::units::*;
use crate::device::{Device, Program, Texture, TextureSlot, VertexDescriptor, ShaderError, VAO};
use crate::device::{TextureFilter, VertexAttribute, VertexAttributeKind, VertexUsageHint};
use euclid::Transform3D;
use crate::internal_types::Swizzle;
use std::f32;
use log::error;
use std::convert::TryInto;

#[derive(Debug, Copy, Clone)]
enum GLCursorSampler {
    Cursor,
}

impl Into<TextureSlot> for GLCursorSampler {
    fn into(self) -> TextureSlot {
        match self {
            GLCursorSampler::Cursor => TextureSlot(0),
        }
    }
}

const DESC_CURSOR: VertexDescriptor = VertexDescriptor {
    vertex_attributes: &[
        VertexAttribute {
            name: "aPosition",
            count: 2,
            kind: VertexAttributeKind::F32,
        },
        VertexAttribute {
            name: "aColorTexCoord",
            count: 2,
            kind: VertexAttributeKind::F32,
        },
    ],
    instance_attributes: &[],
};

#[repr(C)]
#[derive(Debug)]
pub struct GLCursorVertex {
    pub x: f32,
    pub y: f32,
    pub u: f32,
    pub v: f32,
}

impl GLCursorVertex {
    pub fn new(x: f32, y: f32, u: f32, v: f32) -> GLCursorVertex {
        GLCursorVertex { x, y, u, v }
    }
}

pub struct GLCursorRenderer {
    glcursor_vertices: Vec<GLCursorVertex>,
    glcursor_indices: Vec<u32>,
    glcursor_vao: VAO,
    glcursor_program: Program,
    glcursor_texture: Option<Texture>,
    glcursor_enable: bool,
    glcursor_x: i32,
    glcursor_y: i32,
    glcursor_width: i32,
    glcursor_height: i32,
    glcursor_buffer: Vec<u8>,
}

impl GLCursorRenderer {
    pub fn new(device: &mut Device) -> Result<Self, ShaderError> {
        let glcursor_program = device.create_program_linked("glcursor_color", &[], &DESC_CURSOR)?;

        device.bind_program(&glcursor_program);
        device.bind_shader_samplers(&glcursor_program, &[("sColor0", GLCursorSampler::Cursor)]);

        let glcursor_vao = device.create_vao(&DESC_CURSOR, 1);

        Ok(GLCursorRenderer {
            glcursor_vertices: Vec::new(),
            glcursor_indices: Vec::new(),
            glcursor_vao,
            glcursor_program,
            glcursor_texture: None,
            glcursor_enable: false,
            glcursor_x: -1,
            glcursor_y: -1,
            glcursor_width: 0,
            glcursor_height: 0,
            glcursor_buffer: Vec::new(),
        })
    }

    pub fn deinit(mut self, device: &mut Device) {
        if let Some(t) = self.glcursor_texture.take() {
            device.delete_texture(t);
        }
        device.delete_program(self.glcursor_program);
        device.delete_vao(self.glcursor_vao);
    }

    pub fn set_glcursor(
        &mut self,
        device: &mut Device,
        enable: bool,
        x: i32,
        y: i32,
        width: i32,
        height: i32,
        cursor_buffer: *mut u8,
        size: usize,
    ) {
        self.glcursor_vertices.clear();
        self.glcursor_indices.clear();

        let ipw = 1.0 / width as f32;
        let iph = 1.0 / height as f32;

        let s0 = 0.0;
        let t0 = 0.0;
        let s1 = width as f32 * ipw;
        let t1 = height as f32 * iph;

        self.glcursor_vertices
            .push(GLCursorVertex::new(x as f32, y as f32, s0, t0));
        self.glcursor_vertices
            .push(GLCursorVertex::new((x + width) as f32, y as f32, s1, t0));
        self.glcursor_vertices
            .push(GLCursorVertex::new(x as f32, (y + height) as f32, s0, t1));
        self.glcursor_vertices.push(GLCursorVertex::new(
            (x + width) as f32,
            (y + height) as f32,
            s1,
            t1,
        ));

        self.glcursor_indices.push(0);
        self.glcursor_indices.push(1);
        self.glcursor_indices.push(2);
        self.glcursor_indices.push(2);
        self.glcursor_indices.push(1);
        self.glcursor_indices.push(3);

        if !cursor_buffer.is_null() {
            self.glcursor_buffer.clear();
            self.glcursor_buffer = Vec::with_capacity(size);

            unsafe {
                std::ptr::copy_nonoverlapping(
                    cursor_buffer,
                    self.glcursor_buffer.as_mut_ptr(),
                    size,
                );
                self.glcursor_buffer.set_len(size); // Set the length of the Vec to the number of copied elements
            }

            if let Some(t) = self.glcursor_texture.take() {
                device.delete_texture(t);
            }

            let texture = device.create_texture(
                ImageBufferKind::Texture2D,
                ImageFormat::BGRA8,
                width,
                height,
                TextureFilter::Linear,
                None,
            );
            device.upload_texture_immediate(&texture, &self.glcursor_buffer);
            self.glcursor_texture = Some(texture);
        }

        self.glcursor_enable = enable;
        self.glcursor_x = x;
        self.glcursor_y = y;
        self.glcursor_width = width;
        self.glcursor_height = height;
    }

    pub fn render(
        &mut self,
        device: &mut Device,
        viewport_size: Option<DeviceIntSize>,
        scale: f32,
        surface_origin_is_top_left: bool,
    ) {
        if !self.glcursor_enable {
            return;
        }

        if let Some(viewport_size) = viewport_size {
            device.disable_depth();
            device.set_blend(true);
            device.set_blend_mode_premultiplied_alpha();

            let (bottom, top) = if surface_origin_is_top_left {
                (0.0, viewport_size.height as f32 * scale)
            } else {
                (viewport_size.height as f32 * scale, 0.0)
            };

            let projection = Transform3D::ortho(
                0.0,
                viewport_size.width as f32 * scale,
                bottom,
                top,
                device.ortho_near_plane(),
                device.ortho_far_plane(),
            );

            // draw cursor
            if !self.glcursor_vertices.is_empty() {
                device.bind_program(&self.glcursor_program);
                device.set_uniforms(&self.glcursor_program, &projection);

                match self.glcursor_texture.as_mut() {
                    Some(t) => {
                        device.bind_texture(GLCursorSampler::Cursor, &t, Swizzle::Bgra);
                    }
                    None => {}
                }
                device.bind_vao(&self.glcursor_vao);
                device.update_vao_indices(
                    &self.glcursor_vao,
                    &self.glcursor_indices,
                    VertexUsageHint::Dynamic,
                );
                device.update_vao_main_vertices(
                    &self.glcursor_vao,
                    &self.glcursor_vertices,
                    VertexUsageHint::Dynamic,
                );
                device.draw_triangles_u32(0, self.glcursor_indices.len() as i32);
            }
        }
    }
}

pub struct LazyInitializedGLCursorRenderer {
    glcursor_renderer: Option<GLCursorRenderer>,
    failed: bool,
    glcursor_enable: bool,
}

impl LazyInitializedGLCursorRenderer {
    pub fn new() -> Self {
        Self {
            glcursor_renderer: None,
            failed: false,
            glcursor_enable: false,
        }
    }

    pub fn create_render(&mut self, device: &mut Device) -> bool {
        if self.glcursor_renderer.is_none() {
            match GLCursorRenderer::new(device) {
                Ok(renderer) => {
                    self.glcursor_renderer = Some(renderer);
                }
                Err(_) => {
                    error!("Can not create GLCursor Render");
                    self.failed = true;
                    return false;
                }
            }
        }
        true
    }

    pub fn set_glcursor(
        &mut self,
        device: &mut Device,
        enable: bool,
        x: i32,
        y: i32,
        width: i32,
        height: i32,
        cursor_buffer: *mut u8,
        size: usize,
    ) {
        if enable && !self.glcursor_enable {
            if !self.create_render(device) {
                return;
            }
        }

        self.glcursor_enable = enable;
        self.glcursor_renderer
            .as_mut()
            .expect("glcursor render should exist")
            .set_glcursor(device, enable, x, y, width, height, cursor_buffer, size);
    }

    pub fn get_mut<'a>(&'a mut self, device: &mut Device) -> Option<&'a mut GLCursorRenderer> {
        if self.failed || !self.glcursor_enable {
            return None;
        }

        if self.glcursor_renderer.is_none() {
            match GLCursorRenderer::new(device) {
                Ok(renderer) => {
                    self.glcursor_renderer = Some(renderer);
                }
                Err(_) => {
                    // The shader compilation code already logs errors.
                    self.failed = true;
                }
            }
        }

        self.glcursor_renderer.as_mut()
    }

    /// Returns mut ref to `GLCursor::GLCursorRenderer` if one already exists, otherwise returns `None`.
    pub fn try_get_mut<'a>(&'a mut self) -> Option<&'a mut GLCursorRenderer> {
        self.glcursor_renderer.as_mut()
    }

    pub fn deinit(self, device: &mut Device) {
        if let Some(glcursor_renderer) = self.glcursor_renderer {
            glcursor_renderer.deinit(device);
        }
    }
}
