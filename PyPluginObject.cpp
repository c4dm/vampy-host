/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    VampyHost

    Use Vamp audio analysis plugins in Python

    Gyorgy Fazekas and Chris Cannam
    Centre for Digital Music, Queen Mary, University of London
    Copyright 2008-2014 Queen Mary, University of London
  
    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use, copy,
    modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
    ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
    CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

    Except as contained in this notice, the names of the Centre for
    Digital Music; Queen Mary, University of London; and the authors
    shall not be used in advertising or otherwise to promote the sale,
    use or other dealings in this Software without prior written
    authorization.
*/

#include "PyPluginObject.h"

// define a unique API pointer 
#define PY_ARRAY_UNIQUE_SYMBOL VAMPYHOST_ARRAY_API
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#define NO_IMPORT_ARRAY
#include "numpy/arrayobject.h"

#include "VectorConversion.h"
#include "PyRealTime.h"

#include <string>
#include <vector>

using namespace std;
using namespace Vamp;

static void
PyPluginObject_dealloc(PyPluginObject *self)
{
    cerr << "PyPluginObject_dealloc" << endl;
    delete self->plugin;
    PyObject_Del(self);
}

PyDoc_STRVAR(xx_foo_doc, "Some description"); //!!!

//!!! todo: conv errors

static
PyPluginObject *
getPluginObject(PyObject *pyPluginHandle)
{
    cerr << "getPluginObject" << endl;

    PyPluginObject *pd = 0;
    if (PyPlugin_Check(pyPluginHandle)) {
        pd = (PyPluginObject *)pyPluginHandle;
    }
    if (!pd || !pd->plugin) {
        PyErr_SetString(PyExc_AttributeError,
			"Invalid or already deleted plugin handle.");
        return 0;
    } else {
        return pd;
    }
}

PyObject *
PyPluginObject_From_Plugin(Plugin *plugin)
{
    PyPluginObject *pd = 
        (PyPluginObject *)PyType_GenericAlloc(&Plugin_Type, 0);
    pd->plugin = plugin;
    pd->isInitialised = false;
    pd->channels = 0;
    pd->blockSize = 0;
    pd->stepSize = 0;
    return (PyObject *)pd;
}

static PyObject *
vampyhost_initialise(PyObject *self, PyObject *args)
{
    cerr << "vampyhost_initialise" << endl;
    
    size_t channels, blockSize, stepSize;

    if (!PyArg_ParseTuple (args, "nnn",
			   (size_t) &channels,
			   (size_t) &stepSize,
			   (size_t) &blockSize)) {
	PyErr_SetString(PyExc_TypeError,
			"initialise() takes channel count, step size, and block size arguments");
	return 0;
    }

    PyPluginObject *pd = getPluginObject(self);
    if (!pd) return 0;

    pd->channels = channels;
    pd->stepSize = stepSize;
    pd->blockSize = blockSize;

    if (!pd->plugin->initialise(channels, stepSize, blockSize)) {
        cerr << "Failed to initialise native plugin adapter with channels = " << channels << ", stepSize = " << stepSize << ", blockSize = " << blockSize << " and ADAPT_ALL_SAFE set" << endl;
	PyErr_SetString(PyExc_TypeError,
			"Plugin initialization failed");
	return 0;
    }

    pd->isInitialised = true;

    return Py_True;
}

static PyObject *
vampyhost_reset(PyObject *self, PyObject *)
{
    cerr << "vampyhost_reset" << endl;
    
    PyPluginObject *pd = getPluginObject(self);
    if (!pd) return 0;

    if (!pd->isInitialised) {
        PyErr_SetString(PyExc_StandardError,
                        "Plugin has not been initialised");
        return 0;
    }
        
    pd->plugin->reset();
    return Py_True;
}

static PyObject *
vampyhost_getParameter(PyObject *self, PyObject *args)
{
    cerr << "vampyhost_getParameter" << endl;

    PyObject *pyParam;

    if (!PyArg_ParseTuple(args, "S", &pyParam)) {
	PyErr_SetString(PyExc_TypeError,
			"getParameter() takes parameter id (string) argument");
	return 0; }

    PyPluginObject *pd = getPluginObject(self);
    if (!pd) return 0;

    float value = pd->plugin->getParameter(PyString_AS_STRING(pyParam));
    return PyFloat_FromDouble(double(value));
}

static PyObject *
vampyhost_setParameter(PyObject *self, PyObject *args)
{
    cerr << "vampyhost_setParameter" << endl;

    PyObject *pyParam;
    float value;

    if (!PyArg_ParseTuple(args, "Sf", &pyParam, &value)) {
	PyErr_SetString(PyExc_TypeError,
			"setParameter() takes parameter id (string), and value (float) arguments");
	return 0; }

    PyPluginObject *pd = getPluginObject(self);
    if (!pd) return 0;

    pd->plugin->setParameter(PyString_AS_STRING(pyParam), value);
    return Py_True;
}

static PyObject *
vampyhost_process(PyObject *self, PyObject *args)
{
    cerr << "vampyhost_process" << endl;

    PyObject *pyBuffer;
    PyObject *pyRealTime;

    if (!PyArg_ParseTuple(args, "OO",
			  &pyBuffer,			// Audio data
			  &pyRealTime)) {		// TimeStamp
	PyErr_SetString(PyExc_TypeError,
			"process() takes plugin handle (object), buffer (2D array of channels * samples floats) and timestamp (RealTime) arguments");
	return 0; }

    if (!PyRealTime_Check(pyRealTime)) {
	PyErr_SetString(PyExc_TypeError,"Valid timestamp required.");
	return 0; }

    if (!PyList_Check(pyBuffer)) {
	PyErr_SetString(PyExc_TypeError, "List of NumPy Array required for process input.");
        return 0;
    }

    PyPluginObject *pd = getPluginObject(self);
    if (!pd) return 0;

    if (!pd->isInitialised) {
	PyErr_SetString(PyExc_StandardError,
			"Plugin has not been initialised.");
	return 0;
    }

    int channels =  pd->channels;

    if (PyList_GET_SIZE(pyBuffer) != channels) {
        cerr << "Wrong number of channels: got " << PyList_GET_SIZE(pyBuffer) << ", expected " << channels << endl;
	PyErr_SetString(PyExc_TypeError, "Wrong number of channels");
        return 0;
    }

    float **inbuf = new float *[channels];

    VectorConversion typeConv;

    cerr << "here!"  << endl;
    
    vector<vector<float> > data;
    for (int c = 0; c < channels; ++c) {
        PyObject *cbuf = PyList_GET_ITEM(pyBuffer, c);
        data.push_back(typeConv.PyValue_To_FloatVector(cbuf));
    }
    
    for (int c = 0; c < channels; ++c) {
        if (data[c].size() != pd->blockSize) {
            cerr << "Wrong number of samples on channel " << c << ": expected " << pd->blockSize << " (plugin's block size), got " << data[c].size() << endl;
            PyErr_SetString(PyExc_TypeError, "Wrong number of samples");
            return 0;
        }
        inbuf[c] = &data[c][0];
    }

    cerr << "no, here!"  << endl;

    RealTime timeStamp = *PyRealTime_AsRealTime(pyRealTime);

    cerr << "no no, here!"  << endl;

    Plugin::FeatureSet fs = pd->plugin->process(inbuf, timeStamp);

    delete[] inbuf;

    cerr << "no no no, here!"  << endl;
    
    VectorConversion conv;
    
    PyObject *pyFs = PyDict_New();

    for (Plugin::FeatureSet::const_iterator fsi = fs.begin();
         fsi != fs.end(); ++fsi) {

        int fno = fsi->first;
        const Plugin::FeatureList &fl = fsi->second;

        if (!fl.empty()) {

            PyObject *pyFl = PyList_New(fl.size());

            for (int fli = 0; fli < (int)fl.size(); ++fli) {

                const Plugin::Feature &f = fl[fli];
                PyObject *pyF = PyDict_New();

                if (f.hasTimestamp) {
                    PyDict_SetItemString
                        (pyF, "timestamp", PyRealTime_FromRealTime(f.timestamp));
                }
                if (f.hasDuration) {
                    PyDict_SetItemString
                        (pyF, "duration", PyRealTime_FromRealTime(f.duration));
                }

                PyDict_SetItemString
                    (pyF, "label", PyString_FromString(f.label.c_str()));

                if (!f.values.empty()) {
                    PyDict_SetItemString
                        (pyF, "values", conv.PyArray_From_FloatVector(f.values));
                }

                PyList_SET_ITEM(pyFl, fli, pyF);
            }

            PyObject *pyN = PyInt_FromLong(fno);
            PyDict_SetItem(pyFs, pyN, pyFl);
        }
    }

    cerr << "no you fool, here!"  << endl;
    
    return pyFs;
}

static PyObject *
vampyhost_unload(PyObject *self, PyObject *)
{
    cerr << "vampyhost_unloadPlugin" << endl;
    
    PyPluginObject *pd = getPluginObject(self);
    if (!pd) return 0;

    delete pd->plugin;
    pd->plugin = 0; // This is checked by getPluginObject, so we 
                    // attempt to avoid repeated calls from blowing up

    return Py_True;
}

static PyMethodDef PyPluginObject_methods[] =
{
    {"getParameter",	vampyhost_getParameter, METH_VARARGS,
     xx_foo_doc}, //!!! fix all these!

    {"setParameter",	vampyhost_setParameter, METH_VARARGS,
     xx_foo_doc},

    {"initialise",	vampyhost_initialise, METH_VARARGS,
     xx_foo_doc},

    {"reset",	vampyhost_reset, METH_NOARGS,
     xx_foo_doc},

    {"process",	vampyhost_process, METH_VARARGS,
     xx_foo_doc},

    {"unload", vampyhost_unload, METH_NOARGS,
     xx_foo_doc},
    
    {0, 0}
};

static int
PyPluginObject_setattr(PyPluginObject *self, char *name, PyObject *value)
{
    return -1;
}

static PyObject *
PyPluginObject_getattr(PyPluginObject *self, char *name)
{
    return Py_FindMethod(PyPluginObject_methods, (PyObject *)self, name);
}

/* Doc:: 10.3 Type Objects */ /* static */ 
PyTypeObject Plugin_Type = 
{
    PyObject_HEAD_INIT(NULL)
    0,						/*ob_size*/
    "vampyhost.Plugin",				/*tp_name*/
    sizeof(PyPluginObject),	/*tp_basicsize*/
    0,		/*tp_itemsize*/
    (destructor)PyPluginObject_dealloc, /*tp_dealloc*/
    0,						/*tp_print*/
    (getattrfunc)PyPluginObject_getattr, /*tp_getattr*/
    (setattrfunc)PyPluginObject_setattr, /*tp_setattr*/
    0,						/*tp_compare*/
    0,			/*tp_repr*/
    0,	/*tp_as_number*/
    0,						/*tp_as_sequence*/
    0,						/*tp_as_mapping*/
    0,						/*tp_hash*/
    0,                      /*tp_call*/
    0,                      /*tp_str*/
    0,                      /*tp_getattro*/
    0,                      /*tp_setattro*/
    0,                      /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,     /*tp_flags*/
    "Plugin Object",      /*tp_doc*/
    0,                      /*tp_traverse*/
    0,                      /*tp_clear*/
    0,                      /*tp_richcompare*/
    0,                      /*tp_weaklistoffset*/
    0,                      /*tp_iter*/
    0,                      /*tp_iternext*/
    PyPluginObject_methods,       /*tp_methods*/ 
    0,                      /*tp_members*/
    0,                      /*tp_getset*/
    0,                      /*tp_base*/
    0,                      /*tp_dict*/
    0,                      /*tp_descr_get*/
    0,                      /*tp_descr_set*/
    0,                      /*tp_dictoffset*/
    0,                      /*tp_init*/
    PyType_GenericAlloc,         /*tp_alloc*/
    0,           /*tp_new*/
    PyObject_Del,			/*tp_free*/
    0,                      /*tp_is_gc*/
};
