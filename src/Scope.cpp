#include "Types.hpp"

#include "InlineMethods.hpp"

PyObject * MGLContext_scope(MGLContext * self, PyObject * args) {
	MGLFramebuffer * framebuffer;
	PyObject * enable_flags;
	PyObject * textures;
	PyObject * uniform_buffers;
	PyObject * shader_storage_buffers;

	int args_ok = PyArg_ParseTuple(
		args,
		"O!OOOO",
		&MGLFramebuffer_Type,
		&framebuffer,
		&enable_flags,
		&textures,
		&uniform_buffers,
		&shader_storage_buffers
	);

	if (!args_ok) {
		return 0;
	}

	int flags = MGL_INVALID;
	if (enable_flags != Py_None) {
		flags = PyLong_AsLong(enable_flags);
		if (PyErr_Occurred()) {
			MGLError_Set("invalid enable_flags");
			return 0;
		}
	}

	MGLScope * scope = (MGLScope *)MGLScope_Type.tp_alloc(&MGLScope_Type, 0);

	Py_INCREF(self);
	scope->context = self;

	scope->enable_flags = flags;

	Py_INCREF(framebuffer);
	scope->framebuffer = framebuffer;

	Py_INCREF(self->bound_framebuffer);
	scope->old_framebuffer = self->bound_framebuffer;

	int num_textures = PyTuple_Size(textures);
	int num_uniform_buffers = PyTuple_Size(uniform_buffers);
	int num_shader_storage_buffers = PyTuple_Size(shader_storage_buffers);

	scope->num_textures = num_textures;
	scope->textures = new int[scope->num_textures * 3];
	scope->num_buffers = num_uniform_buffers + num_shader_storage_buffers;
	scope->buffers = new int[scope->num_buffers * 3];

	for (int i = 0; i < num_textures; ++i) {
		PyObject * tup = PyTuple_GET_ITEM(textures, i);
		PyObject * item = PyTuple_GET_ITEM(tup, 0);

		int texture_type;
		int texture_obj;

		if (Py_TYPE(item) == &MGLTexture_Type) {
			MGLTexture * texture = (MGLTexture *)item;
			texture_type = texture->samples ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
			texture_obj = texture->texture_obj;
		} else if (Py_TYPE(item) == &MGLTexture3D_Type) {
			MGLTexture3D * texture = (MGLTexture3D *)item;
			texture_type = GL_TEXTURE_3D;
			texture_obj = texture->texture_obj;
		} else if (Py_TYPE(item) == &MGLTextureCube_Type) {
			MGLTextureCube * texture = (MGLTextureCube *)item;
			texture_type = GL_TEXTURE_CUBE_MAP;
			texture_obj = texture->texture_obj;
		} else {
			// TODO: fail
			return 0;
		}

		int binding = PyLong_AsLong(PyTuple_GET_ITEM(tup, 1));
		scope->textures[i * 3 + 0] = GL_TEXTURE0 + binding;
		scope->textures[i * 3 + 1] = texture_type;
		scope->textures[i * 3 + 2] = texture_obj;
	}

	for (int i = 0; i < num_uniform_buffers; ++i) {
		PyObject * tup = PyTuple_GET_ITEM(uniform_buffers, i);
		MGLBuffer * buffer = (MGLBuffer *)PyTuple_GET_ITEM(tup, 0);

		if (Py_TYPE(buffer) == &MGLBuffer_Type) {
			int binding = PyLong_AsLong(PyTuple_GET_ITEM(tup, 1));
			scope->buffers[i * 3 + 0] = GL_UNIFORM_BUFFER;
			scope->buffers[i * 3 + 1] = buffer->buffer_obj;
			scope->buffers[i * 3 + 2] = binding;
		} else {
			// TODO: fail
		}
	}

	int base = num_uniform_buffers * 3;

	for (int i = 0; i < num_shader_storage_buffers; ++i) {
		PyObject * tup = PyTuple_GET_ITEM(shader_storage_buffers, i);
		MGLBuffer * buffer = (MGLBuffer *)PyTuple_GET_ITEM(tup, 0);

		if (Py_TYPE(buffer) == &MGLBuffer_Type) {
			int binding = PyLong_AsLong(PyTuple_GET_ITEM(tup, 1));
			scope->buffers[base + i * 3 + 0] = GL_SHADER_STORAGE_BUFFER;
			scope->buffers[base + i * 3 + 1] = buffer->buffer_obj;
			scope->buffers[base + i * 3 + 2] = binding;
		} else {
			// TODO: fail
		}
	}

	return (PyObject *)scope;
}

PyObject * MGLScope_tp_new(PyTypeObject * type, PyObject * args, PyObject * kwargs) {
	MGLScope * self = (MGLScope *)type->tp_alloc(type, 0);

	if (self) {
		self->textures = 0;
		self->buffers = 0;
	}

	return (PyObject *)self;
}

void MGLScope_tp_dealloc(MGLScope * self) {
	MGLScope_Type.tp_free((PyObject *)self);
}

extern PyObject * MGLFramebuffer_use(MGLFramebuffer * self);

PyObject * MGLScope_begin(MGLScope * self, PyObject * args) {
	int args_ok = PyArg_ParseTuple(
		args,
		""
	);

	if (!args_ok) {
		return 0;
	}

	const GLMethods & gl = self->context->gl;
	const int & flags = self->enable_flags;

	MGLFramebuffer_use(self->framebuffer);

	for (int i = 0; i < self->num_textures; ++i) {
		gl.ActiveTexture(self->textures[i * 3]);
		gl.BindTexture(self->textures[i * 3 + 1], self->textures[i * 3 + 2]);
	}

	for (int i = 0; i < self->num_buffers; ++i) {
		gl.BindBufferBase(self->buffers[i * 3], self->buffers[i * 3 + 1], self->buffers[i * 3 + 2]);
	}

	if (flags & MGL_BLEND) {
		gl.Enable(GL_BLEND);
	} else {
		gl.Disable(GL_BLEND);
	}

	if (flags & MGL_DEPTH_TEST) {
		gl.Enable(GL_DEPTH_TEST);
	} else {
		gl.Disable(GL_DEPTH_TEST);
	}

	if (flags & MGL_CULL_FACE) {
		gl.Enable(GL_CULL_FACE);
	} else {
		gl.Disable(GL_CULL_FACE);
	}

	if (flags & MGL_RASTERIZER_DISCARD) {
		gl.Enable(GL_RASTERIZER_DISCARD);
	} else {
		gl.Disable(GL_RASTERIZER_DISCARD);
	}

	Py_RETURN_NONE;
}

PyObject * MGLScope_end(MGLScope * self, PyObject * args) {
	int args_ok = PyArg_ParseTuple(
		args,
		""
	);

	if (!args_ok) {
		return 0;
	}

	const GLMethods & gl = self->context->gl;
	const int & flags = self->old_enable_flags;

	MGLFramebuffer_use(self->old_framebuffer);

	if (flags & MGL_BLEND) {
		gl.Enable(GL_BLEND);
	} else {
		gl.Disable(GL_BLEND);
	}

	if (flags & MGL_DEPTH_TEST) {
		gl.Enable(GL_DEPTH_TEST);
	} else {
		gl.Disable(GL_DEPTH_TEST);
	}

	if (flags & MGL_CULL_FACE) {
		gl.Enable(GL_CULL_FACE);
	} else {
		gl.Disable(GL_CULL_FACE);
	}

	if (flags & MGL_RASTERIZER_DISCARD) {
		gl.Enable(GL_RASTERIZER_DISCARD);
	} else {
		gl.Disable(GL_RASTERIZER_DISCARD);
	}

	Py_RETURN_NONE;
}

PyMethodDef MGLScope_tp_methods[] = {
	{"begin", (PyCFunction)MGLScope_begin, METH_VARARGS, 0},
	{"end", (PyCFunction)MGLScope_end, METH_VARARGS, 0},
	// {"release", (PyCFunction)MGLScope_release, METH_NOARGS, 0},
	{0},
};

PyGetSetDef MGLScope_tp_getseters[] = {
	{0},
};

PyTypeObject MGLScope_Type = {
	PyVarObject_HEAD_INIT(0, 0)
	"mgl.Scope",                                            // tp_name
	sizeof(MGLScope),                                       // tp_basicsize
	0,                                                      // tp_itemsize
	(destructor)MGLScope_tp_dealloc,                        // tp_dealloc
	0,                                                      // tp_print
	0,                                                      // tp_getattr
	0,                                                      // tp_setattr
	0,                                                      // tp_reserved
	0,                                                      // tp_repr
	0,                                                      // tp_as_number
	0,                                                      // tp_as_sequence
	0,                                                      // tp_as_mapping
	0,                                                      // tp_hash
	0,                                                      // tp_call
	0,                                                      // tp_str
	0,                                                      // tp_getattro
	0,                                                      // tp_setattro
	0,                                                      // tp_as_buffer
	Py_TPFLAGS_DEFAULT,                                     // tp_flags
	0,                                                      // tp_doc
	0,                                                      // tp_traverse
	0,                                                      // tp_clear
	0,                                                      // tp_richcompare
	0,                                                      // tp_weaklistoffset
	0,                                                      // tp_iter
	0,                                                      // tp_iternext
	MGLScope_tp_methods,                                    // tp_methods
	0,                                                      // tp_members
	MGLScope_tp_getseters,                                  // tp_getset
	0,                                                      // tp_base
	0,                                                      // tp_dict
	0,                                                      // tp_descr_get
	0,                                                      // tp_descr_set
	0,                                                      // tp_dictoffset
	0,                                                      // tp_init
	0,                                                      // tp_alloc
	MGLScope_tp_new,                                        // tp_new
};

void MGLScope_Invalidate(MGLScope * scope) {
	if (Py_TYPE(scope) == &MGLInvalidObject_Type) {
		return;
	}

	// TODO: decref

	// const GLMethods & gl = scope->context->gl;

	// TODO: release

	Py_DECREF(scope->context);
	Py_TYPE(scope) = &MGLInvalidObject_Type;
	Py_DECREF(scope);
}
